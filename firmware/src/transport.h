#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <mqtt_client.h>
#include <cJSON.h>
#include <atomic>
namespace transport {
static const char* SERVICE="aa490001-31c7-4e70-b655-12c923ec1a01";
static const char* EVENT="aa490002-31c7-4e70-b655-12c923ec1a01";
static const char* DETAIL="aa490003-31c7-4e70-b655-12c923ec1a01";
inline String field(cJSON* j,const char* key){auto v=cJSON_GetObjectItemCaseSensitive(j,key);return cJSON_IsString(v)?String(v->valuestring):String();}
inline String jsonText(cJSON* j){char* p=cJSON_PrintUnformatted(j);String s=p?p:"{}";cJSON_free(p);cJSON_Delete(j);return s;}
struct Event {uint32_t seq,time;uint8_t label;float distance,margin;char id[16],name[64];};
class Link {
 Preferences prefs;String ssid,pass,host,user,password,topic="mozhang/gesture",deviceId;int port=1883;
 QueueHandle_t queue=nullptr;esp_mqtt_client_handle_t client=nullptr;BLECharacteristic *eventChar=nullptr,*detailChar=nullptr;
 std::atomic<bool> online{false},advertiseAgain{false};std::atomic<unsigned> dropped{0};
 uint32_t sequence=0,boot=0;String mode="usb";
 struct Callbacks:BLEServerCallbacks {Link* owner;Callbacks(Link* p):owner(p){}
  void onConnect(BLEServer*)override{owner->online=true;}
  void onDisconnect(BLEServer*)override{owner->online=false;owner->advertiseAgain=true;}
 };
 static void mqttEvent(void* arg,esp_event_base_t,int32_t id,void*){auto p=(Link*)arg;if(id==MQTT_EVENT_CONNECTED)p->online=true;if(id==MQTT_EVENT_DISCONNECTED||id==MQTT_EVENT_ERROR)p->online=false;}
 String payload(const Event& e){auto j=cJSON_CreateObject();cJSON_AddNumberToObject(j,"v",1);cJSON_AddStringToObject(j,"device",deviceId.c_str());cJSON_AddNumberToObject(j,"boot",boot);cJSON_AddNumberToObject(j,"seq",e.seq);cJSON_AddNumberToObject(j,"uptime_ms",e.time);cJSON_AddStringToObject(j,"gesture",e.id);cJSON_AddStringToObject(j,"name",e.name);cJSON_AddNumberToObject(j,"distance",e.distance);return jsonText(j);}
 static void task(void* arg){auto self=(Link*)arg;self->run();}
 void run(){
  if(mode=="mqtt"){
   WiFi.persistent(false);WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(true);WiFi.begin(ssid.c_str(),pass.c_str());
   esp_mqtt_client_config_t c={};c.host=host.c_str();c.port=port;c.client_id=deviceId.c_str();c.username=user.isEmpty()?nullptr:user.c_str();c.password=password.isEmpty()?nullptr:password.c_str();c.keepalive=30;c.network_timeout_ms=2000;c.reconnect_timeout_ms=5000;
   client=esp_mqtt_client_init(&c);if(client){esp_mqtt_client_register_event(client,MQTT_EVENT_ANY,mqttEvent,this);esp_mqtt_client_start(client);}
  }else if(mode=="ble"){
   BLEDevice::init(deviceId.c_str());auto server=BLEDevice::createServer();server->setCallbacks(new Callbacks(this));
   auto service=server->createService(SERVICE);eventChar=service->createCharacteristic(EVENT,BLECharacteristic::PROPERTY_NOTIFY);eventChar->addDescriptor(new BLE2902());
   detailChar=service->createCharacteristic(DETAIL,BLECharacteristic::PROPERTY_READ);detailChar->setValue("{}");service->start();auto ad=BLEDevice::getAdvertising();ad->addServiceUUID(SERVICE);ad->setScanResponse(true);ad->start();
  }
  Event e;
  for(;;){
   if(advertiseAgain.exchange(false))BLEDevice::startAdvertising();
   if(xQueueReceive(queue,&e,pdMS_TO_TICKS(100))!=pdTRUE)continue;
   if(!online || millis()-e.time>2000){dropped++;continue;}
   String data=payload(e);
   if(mode=="mqtt") {if(esp_mqtt_client_publish(client,topic.c_str(),data.c_str(),data.length(),0,false)<0)dropped++;}
   else if(mode=="ble"){
    // 16-byte notification fits even the default 23-byte ATT MTU.
    uint8_t packet[16]={1,e.label};memcpy(packet+2,&e.seq,4);memcpy(packet+6,&e.time,4);
    uint16_t d=uint16_t(e.distance*1000),m=uint16_t(std::min(e.margin,65.f)*1000);memcpy(packet+10,&d,2);memcpy(packet+12,&m,2);
    detailChar->setValue(data.c_str());eventChar->setValue(packet,sizeof(packet));eventChar->notify();
   }
  }
 }
public:
 void begin(){
  boot=esp_random();char id[28];snprintf(id,sizeof(id),"MOZHANG-%06llX",ESP.getEfuseMac()&0xffffffULL);deviceId=id;
  if(prefs.begin("wand-link",false)){
   String raw=prefs.getString("config","{}");auto j=cJSON_Parse(raw.c_str());if(j){String v=field(j,"mode");if(v=="mqtt"||v=="ble")mode=v;ssid=field(j,"ssid");pass=field(j,"wifi_password");host=field(j,"host");user=field(j,"username");password=field(j,"mqtt_password");String t=field(j,"topic");if(t.length())topic=t;auto p=cJSON_GetObjectItem(j,"port");if(cJSON_IsNumber(p))port=p->valueint;cJSON_Delete(j);}
  }
  if(mode!="usb"){queue=xQueueCreate(8,sizeof(Event));if(!queue||xTaskCreate(task,"wand-link",6144,this,1,nullptr)!=pdPASS){mode="usb";dropped++;}}
 }
 bool configure(const String& raw){
  auto j=cJSON_Parse(raw.c_str());if(!j)return false;
  String m=field(j,"mode"),h=field(j,"host"),s=field(j,"ssid"),t=field(j,"topic");auto p=cJSON_GetObjectItem(j,"port");
  bool ok=(m=="usb"||m=="mqtt"||m=="ble") && raw.length()<750;
  if(m=="mqtt")ok=ok&&s.length()>0&&s.length()<=32&&h.length()>0&&h.length()<=128&&t.length()>0&&t.length()<=96&&t.indexOf('#')<0&&t.indexOf('+')<0&&cJSON_IsNumber(p)&&p->valueint>0&&p->valueint<=65535;
  cJSON_Delete(j);if(!ok)return false;return prefs.putString("config",raw)==raw.length();
 }
 String status(){auto j=cJSON_CreateObject();cJSON_AddStringToObject(j,"mode",mode.c_str());cJSON_AddBoolToObject(j,"connected",online);cJSON_AddNumberToObject(j,"dropped",dropped);cJSON_AddStringToObject(j,"device",deviceId.c_str());cJSON_AddStringToObject(j,"ssid",ssid.c_str());cJSON_AddStringToObject(j,"host",host.c_str());cJSON_AddNumberToObject(j,"port",port);cJSON_AddStringToObject(j,"topic",topic.c_str());return jsonText(j);}
 void emit(int label,const char* id,const char* name,float distance,float margin){
  if(mode=="usb")return;Event e={};e.seq=++sequence;e.time=millis();e.label=label;e.distance=distance;e.margin=margin;strlcpy(e.id,id,sizeof(e.id));strlcpy(e.name,name,sizeof(e.name));
  if(!queue||!online||xQueueSend(queue,&e,0)!=pdTRUE)dropped++;
 }
};
}
