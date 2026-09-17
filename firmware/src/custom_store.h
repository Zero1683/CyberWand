#pragma once
#include <LittleFS.h>
#include <esp_partition.h>
#include "recognizer.h"
// Use only the existing unused spiffs partition. Never format nonblank data.
class CustomStore {
 bool ready=false;
 String path(int c,int k){return "/p"+String(c)+"_"+String(k)+".bin";}
public:
 bool begin(){
  if(LittleFS.begin(false)){ready=true;return true;}
  auto part=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"spiffs");
  if(!part)return false;uint8_t bytes[256];
  for(size_t pos=0;pos<part->size;pos+=sizeof(bytes)){
   if(esp_partition_read(part,pos,bytes,sizeof(bytes))!=ESP_OK)return false;
   for(auto b:bytes)if(b!=0xff)return false;
  }
  ready=LittleFS.begin(true);return ready;
 }
 bool load(int c,int k,wand::Pattern& p){if(!ready)return false;auto f=LittleFS.open(path(c,k),"r");return f&&f.size()==sizeof(p)&&f.read((uint8_t*)&p,sizeof(p))==sizeof(p);}
 bool save(int c,int k,const wand::Pattern& p){
  if(!ready)return false;String dest=path(c,k),tmp=dest+".tmp";
  auto f=LittleFS.open(tmp,"w");if(!f)return false;size_t n=f.write((const uint8_t*)&p,sizeof(p));f.flush();f.close();
  if(n!=sizeof(p)){LittleFS.remove(tmp);return false;}return LittleFS.rename(tmp,dest);
 }
 bool clear(int c){if(!ready)return false;bool ok=true;for(int k=0;k<wand::EXAMPLES;k++){auto p=path(c,k);if(LittleFS.exists(p)&&!LittleFS.remove(p))ok=false;}return ok;}
};
