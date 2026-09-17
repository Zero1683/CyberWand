#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <hal/usb_serial_jtag_ll.h>
#include "recognizer.h"
#include "console.h"
#include "custom_store.h"
#include "transport.h"
CustomStore customStore;
transport::Link wandLink;
String displayNames[wand::CLASSES];
bool recordPending=false,trainingAuto=false;
uint32_t recordAt=0,rebootAt=0;
const char* defaultNames[]={"向左","向右","向上","向下","画圆","折线","自定义 1","自定义 2","自定义 3","自定义 4","自定义 5","自定义 6"};
#include "motion_gate.h"
#include "capture_clock.h"
#include "still_window.h"
wand::StillWindow stillWindow;
float residualAcceleration=0;
wand::MotionGate gate;
wand::Frame preRoll[16];int preCount=0,preNext=0;bool autoCapture=false;

BufferedConsole console;
bool streaming=false;
float liveMotion[6]={};

constexpr int GREEN=7,RED=10,KEY=3;
wand::Library library;
wand::Frame frames[wand::MAX_SAMPLES];
wand::Pattern lastPattern;
Preferences prefs;
bool imuOK=false,calibrated=false,capturing=false,locked=false,lastValid=false,storageOK=false;
uint8_t address=0x6A;
uint32_t errors=0,samples=0,lastSample=0,lastMicros=0,captureStart=0,lastReport=0;
uint32_t feedbackStart=0; int feedback=0,training=-1,count=0;
float bias[3]={},gravity[3]={},filtered[6]={},travel=0,accEnergy=0;
float sum[6]={},squareSum[6]={}; int calibrationCount=0;
uint32_t calibrationStart=0;
bool badCapture=false; String commandLine;

bool readReg(uint8_t reg,uint8_t* out,uint8_t n) {
  Wire.beginTransmission(address);Wire.write(reg);
  if(Wire.endTransmission(false) || Wire.requestFrom(address,n)!=n) {errors++;return false;}
  for(int i=0;i<n;i++) out[i]=Wire.read(); return true;
}
bool writeReg(uint8_t reg,uint8_t value) {
  Wire.beginTransmission(address);Wire.write(reg);Wire.write(value);
  if(Wire.endTransmission()) {errors++;return false;}return true;
}
bool initializeIMU() {
  uint8_t id=0;
  for(uint8_t a:{uint8_t(0x6A),uint8_t(0x6B)}) {address=a;if(readReg(0x0F,&id,1)&&id==0x6A)break;}
  if(id!=0x6A || !writeReg(0x12,1))return false;
  uint8_t reset=1; uint32_t start=millis();
  do {delay(5);if(!readReg(0x12,&reset,1))return false;}while((reset&1)&&millis()-start<200);
  if(reset&1)return false;
  // 104Hz, +/-4g (0.122mg/LSB), +/-1000dps (35mdps/LSB), BDU and auto-increment.
  // Higher range than diagnostics avoids clipping during deliberate wand swings.
  if(!writeReg(0x10,0x48)||!writeReg(0x11,0x48)||!writeReg(0x12,0x44)||!writeReg(0x0D,0)||!writeReg(0x0E,0))return false;
  uint8_t ctrl[3],irq[2];
  return readReg(0x10,ctrl,3)&&ctrl[0]==0x48&&ctrl[1]==0x48&&ctrl[2]==0x44&&readReg(0x0D,irq,2)&&irq[0]==0&&irq[1]==0;
}
void calibrate() {
  stillWindow.reset();gate.reset(millis());preCount=preNext=0;calibrated=false;calibrationCount=0;calibrationStart=millis();
  memset(sum,0,sizeof(sum));memset(squareSum,0,sizeof(squareSum));
  console.println("CALIBRATING keep still for 2 seconds; movement restarts window");
}
void indicate(int kind) {feedback=kind;feedbackStart=millis();}
void labelStatus(int c){auto j=cJSON_CreateObject();cJSON_AddStringToObject(j,"id",wand::names[c]);cJSON_AddStringToObject(j,"name",displayNames[c].c_str());console.printf("LABEL %s\n",transport::jsonText(j).c_str());}
const char* recordPhase(){return training<0?"recognize":!trainingAuto?"manual":!recordPending?"review":capturing?"capture":int32_t(millis()-recordAt)<0?"countdown":gate.armed?"armed":"settling";}
void status() {
  console.printf("STATUS calibrated=%d IMU=%s samples=%lu errors=%lu train=%d storage=%d capturing=%d drops=%lu\n",calibrated,imuOK?"OK":"FAIL",(unsigned long)samples,(unsigned long)errors,training,storageOK,capturing,(unsigned long)console.drops());
  for(int i=0;i<wand::CLASSES;i++)console.printf("TEMPLATES %s %u/3\n",wand::names[i],library.counts[i]);
  for(int i=0;i<wand::CLASSES;i++)labelStatus(i);
  console.printf("LINK %s\n",wandLink.status().c_str());
}
void savePattern(int c) {
  if(!storageOK) {console.println("SAVE FAILED storage unavailable");indicate(-1);return;}
  if(library.counts[c]>=wand::EXAMPLES) {console.println("TRAIN FULL use clear <name> before replacement");return;}
  int slot=library.counts[c];char key[12];snprintf(key,sizeof(key),"p%d_%d",c,slot);
  if(c>=6?!customStore.save(c,slot,lastPattern):prefs.putBytes(key,&lastPattern,sizeof(lastPattern))!=sizeof(lastPattern)) {console.println("SAVE FAILED");indicate(-1);return;}
  library.patterns[c][slot]=lastPattern;library.counts[c]++;
  console.printf("TRAIN SAVED %s %d/3\n",wand::names[c],library.counts[c]);indicate(1);
  if(library.counts[c]==wand::EXAMPLES) {console.println("TRAIN COMPLETE select next label or recognize");}
}
void finishCapture() {
  capturing=false;recordPending=false;gate.reset(millis());preCount=preNext=0;
  uint32_t duration=wand::captureElapsed(millis(),captureStart);
  float rms=count?std::sqrt(accEnergy/count):0;
  if(badCapture||duration<250||duration>4000||count<20||(travel<8 && rms<0.06f)||!wand::normalize(frames,count,lastPattern)) {
    lastValid=false;console.printf("REJECT capture duration=%lu n=%d travel=%.1f accel_rms=%.3f bad=%d\n",(unsigned long)duration,count,travel,rms,badCapture);indicate(-1);return;
  }
  lastValid=true;
  console.printf("CAPTURE END duration=%lu n=%d travel=%.1f accel_rms=%.3f\n",(unsigned long)duration,count,travel,rms);
  if(training>=0) {savePattern(training);return;}
  auto r=library.classify(lastPattern);
  console.printf("RESULT %s distance=%.3f margin=%.3f (not probability)\n",r.label<0?"unknown":wand::names[r.label],r.distance,r.margin);
  if(r.label>=0)wandLink.emit(r.label,wand::names[r.label],displayNames[r.label].c_str(),r.distance,r.margin);
  indicate(r.label<0?-1:1);
}
void startCapture(bool automatic=false) {
  if(!imuOK||!calibrated||!samples||millis()-lastSample>100) {console.println("REJECT IMU not ready");indicate(-1);return;}
  if(training>=0&&library.counts[training]>=wand::EXAMPLES) {console.println("REJECT class full");return;}
  capturing=true;captureStart=millis();count=0;travel=0;accEnergy=0;badCapture=false;lastValid=false;
  autoCapture=automatic;
  if(automatic){
    for(int k=0;k<preCount;k++)frames[count++]=preRoll[(preNext-preCount+k+16)%16];
    captureStart-=preCount*1000/104;
  }else memset(filtered,0,sizeof(filtered));
  console.printf("CAPTURE START mode=%s trigger=%s\n",training>=0?wand::names[training]:"recognize",automatic?"auto":"button");
}
void sampleIMU(uint32_t now) {
  if(!imuOK)return;
  uint8_t ready,data[12];uint32_t beforeErrors=errors;
  if(!readReg(0x1E,&ready,1)) {if(capturing)badCapture=true;return;}
  if((ready&3)!=3)return;
  if(!readReg(0x22,data,12)) {if(capturing)badCapture=true;return;}
  float x[6];bool clipped=false;
  for(int i=0;i<6;i++) {int16_t raw=int16_t(uint16_t(data[2*i])|(uint16_t(data[2*i+1])<<8));x[i]=raw*(i<3?0.035f:0.000122f);if(abs(int(raw))>32000)clipped=true;}
  uint32_t us=micros();float dt=lastMicros?(us-lastMicros)*1e-6f:1.f/104;lastMicros=us;
  samples++;lastSample=now;
  for(int i=0;i<6;i++)liveMotion[i]=x[i]-(i<3?bias[i]:0);
  if(streaming && samples%4==0 && console.free()>512) console.printf("MOTION %lu,%.2f,%.2f,%.2f,%.4f,%.4f,%.4f\n",(unsigned long)now,liveMotion[0],liveMotion[1],liveMotion[2],liveMotion[3],liveMotion[4],liveMotion[5]);
  if(!calibrated) {
    for(int i=0;i<6;i++){sum[i]+=x[i];squareSum[i]+=x[i]*x[i];}
    if(++calibrationCount>=208) {
      bool still=true;float norm=0;
      for(int i=0;i<6;i++) {
        float mean=sum[i]/calibrationCount,var=squareSum[i]/calibrationCount-mean*mean;
        if(var>(i<3?1.5f*1.5f:0.025f*0.025f))still=false;
        if(i<3){if(fabsf(mean)>15)still=false;}else norm+=mean*mean;
      }
      if(norm<0.85f*0.85f||norm>1.15f*1.15f)still=false;
      if(still) {
        for(int i=0;i<3;i++){bias[i]=sum[i]/calibrationCount;gravity[i]=sum[i+3]/calibrationCount;}
        calibrated=true;console.printf("READY bias_dps=%.3f,%.3f,%.3f AUTO recognition; SW4 for training; help for commands\n",bias[0],bias[1],bias[2]);
      } else {calibrate();}
    }
    return;
  }
  if(dt>0.04f||clipped||errors!=beforeErrors){
    if(capturing)badCapture=true;
    else {gate.reset(now);preCount=preNext=0;}
  }
  if(dt>0.04f)dt=1.f/104;
  // Propagate gravity in the rotating sensor frame (g_dot=-omega cross g).
  float w[3],g[3];for(int i=0;i<3;i++){w[i]=(x[i]-bias[i])*0.01745329252f;g[i]=gravity[i];}
  gravity[0]+=(g[1]*w[2]-g[2]*w[1])*dt;
  gravity[1]+=(g[2]*w[0]-g[0]*w[2])*dt;
  gravity[2]+=(g[0]*w[1]-g[1]*w[0])*dt;
  float gn=std::sqrt(gravity[0]*gravity[0]+gravity[1]*gravity[1]+gravity[2]*gravity[2]);
  if(gn>0.1f)for(float& v:gravity)v/=gn;
  float gyroSpeed2=0;
  for(int i=0;i<3;i++){float gy=x[i]-bias[i];gyroSpeed2+=gy*gy;}
  if(stillWindow.update(now,std::sqrt(gyroSpeed2),x+3))
    for(int i=0;i<3;i++)gravity[i]+=0.08f*(x[i+3]-gravity[i]);
  float speed=0,energy=0;
  for(int i=0;i<3;i++) {
    float gy=x[i]-bias[i],linear=x[i+3]-gravity[i];speed+=gy*gy;energy+=linear*linear;
    filtered[i]+=0.45f*(gy/250.f-filtered[i]);filtered[i+3]+=0.45f*(linear-filtered[i+3]);
  }
  residualAcceleration=std::sqrt(energy);
  if(capturing){
    travel+=std::sqrt(speed)*dt;accEnergy+=energy;
    if(count>=wand::MAX_SAMPLES)badCapture=true;
    else {for(int i=0;i<6;i++)frames[count].v[i]=filtered[i];count++;}
  }else {
    for(int i=0;i<6;i++)preRoll[preNext].v[i]=filtered[i];preNext=(preNext+1)%16;if(preCount<16)preCount++;

  }
  int learned=0;for(int c=0;c<wand::CLASSES;c++)if(library.counts[c])learned++;
  bool autoEnabled=training<0?learned>=2:(trainingAuto&&recordPending&&int32_t(now-recordAt)>=0);
  if(autoEnabled && digitalRead(KEY)==HIGH){
    auto event=gate.update(now,std::sqrt(speed),std::sqrt(energy));
    if(event==wand::MotionGate::Start&&!capturing)startCapture(true);
    if(event==wand::MotionGate::Stop&&capturing&&autoCapture)finishCapture();
  }else if(!capturing)gate.reset(now);
}
int labelFor(String name) {for(int i=0;i<wand::CLASSES;i++)if(name==wand::names[i])return i;return -1;}
void cancelCapture() {
  if(capturing)console.println("CAPTURE CANCEL mode_switch");
  capturing=false;recordPending=false;autoCapture=false;lastValid=false;count=0;preCount=preNext=0;
  gate.reset(millis());locked=digitalRead(KEY)==LOW;
}
void processCommand(String line) {
  line.trim();if(line.isEmpty())return;
  if(line=="stream on" || line=="stream off"){streaming=line=="stream on";console.printf("STREAM %d\n",streaming);return;}
  if(line=="status"){status();return;}
  if(line.startsWith("link ")){cancelCapture();if(wandLink.configure(line.substring(5))){console.println("LINK SAVED restarting");rebootAt=millis()+500;}else console.println("LINK FAILED invalid settings or storage");return;}
  if(line.startsWith("rename ")){
    auto j=cJSON_Parse(line.substring(7).c_str());if(!j){console.println("NAME FAILED invalid JSON");return;}
    int c=labelFor(transport::field(j,"id"));String name=transport::field(j,"name");cJSON_Delete(j);name.trim();bool valid=c>=6&&name.length()>0&&name.length()<=48;
    for(unsigned i=0;i<name.length();i++)if((uint8_t)name[i]<32)valid=false;
    if(!valid){console.println("NAME FAILED invalid name");return;}
    String key="name"+String(c);if(prefs.putString(key.c_str(),name)!=name.length()){console.println("NAME FAILED storage");return;}displayNames[c]=name;labelStatus(c);return;
  }
  if(line.startsWith("record ")){
    int c=labelFor(line.substring(7));if(c<0||!calibrated||library.counts[c]>=wand::EXAMPLES){console.println("REJECT recording unavailable");return;}
    cancelCapture();training=c;trainingAuto=true;recordPending=true;recordAt=millis()+3000;
    console.printf("TRAIN %s automatic one-shot countdown=3\n",wand::names[c]);return;
  }
  if(line=="calibrate"){cancelCapture();calibrate();return;}
  if(line=="recognize"){cancelCapture();training=-1;console.println("MODE recognize AUTO");status();return;}
  else if(line.startsWith("train ")) {
    int c=labelFor(line.substring(6));
    if(c<0)console.println("ERROR unknown label");
    else {cancelCapture();trainingAuto=false;training=c;console.printf("TRAIN %s hold SW4 and perform 3 examples, release after each\n",wand::names[c]);}
  } else if(line.startsWith("clear ")) {
    int c=labelFor(line.substring(6));
    if(c<0||!storageOK)console.println("ERROR label/storage");
    else {
      cancelCapture();
      bool ok=c>=6?customStore.clear(c):true;
      if(c<6)for(int k=0;k<wand::EXAMPLES;k++){char key[12];snprintf(key,sizeof(key),"p%d_%d",c,k);if(prefs.isKey(key)&&!prefs.remove(key))ok=false;}
      if(ok){library.counts[c]=0;console.printf("CLEARED %s\n",wand::names[c]);}else console.println("CLEAR FAILED reboot before training");
    }
  } else if(line=="dump"&&lastValid&&!capturing) {
    console.println("DATA BEGIN time_resampled normalized 48x6 gyroXYZ accelXYZ");
    for(int i=0;i<wand::STEPS;i++)console.printf("DATA %d,%d,%d,%d,%d,%d,%d\n",i,lastPattern.v[i][0],lastPattern.v[i][1],lastPattern.v[i][2],lastPattern.v[i][3],lastPattern.v[i][4],lastPattern.v[i][5]);
    console.println("DATA END");
  } else console.println("COMMANDS help | status | calibrate | train left/right/up/down/circle/zigzag | recognize | clear <label> | dump");
}
void setup() {
  pinMode(GREEN,OUTPUT);pinMode(RED,OUTPUT);pinMode(KEY,INPUT);pinMode(9,INPUT);pinMode(8,INPUT);pinMode(2,INPUT);pinMode(1,INPUT_PULLDOWN);
  usb_serial_jtag_ll_disable_intr_mask(USB_SERIAL_JTAG_LL_INTR_MASK);
  delay(300);console.println("\nMOZHANG GESTURE v0.7 CUSTOM AUTO | personalized six-axis DTW | polling");
  customStore.begin();wandLink.begin();
  storageOK=prefs.begin("wand-gest-v1",false);
  for(int c=0;c<wand::CLASSES;c++){String key="name"+String(c);displayNames[c]=storageOK?prefs.getString(key.c_str(),defaultNames[c]):String(defaultNames[c]);}
  if(storageOK)for(int c=0;c<wand::CLASSES;c++)for(int k=0;k<wand::EXAMPLES;k++) {
    if(c>=6){if(!customStore.load(c,k,library.patterns[c][k]))break;library.counts[c]++;continue;}
    char key[12];snprintf(key,sizeof(key),"p%d_%d",c,k);
    if(!prefs.isKey(key)||prefs.getBytesLength(key)!=sizeof(wand::Pattern))break;
    if(prefs.getBytes(key,&library.patterns[c][k],sizeof(wand::Pattern))!=sizeof(wand::Pattern))break;
    library.counts[c]++;
  }
  Wire.begin(4,5,100000);Wire.setTimeOut(25);imuOK=initializeIMU();
  status();if(imuOK)calibrate();else console.println("FAULT IMU configuration failed");
}
void loop() {
  uint32_t now=millis();console.pump();sampleIMU(now);
  uint8_t input[64];int inputCount=usb_serial_jtag_ll_rxfifo_data_available()?usb_serial_jtag_ll_read_rxfifo(input,sizeof(input)):0;
  for(int i=0;i<inputCount;i++){
    char c=input[i];if(c=='\n'){processCommand(commandLine);commandLine="";}else if(c!='\r') {if(commandLine.length()<800)commandLine+=c;else commandLine="";}
  }
  static int raw=HIGH,stable=HIGH;static uint32_t changed=0;
  int value=digitalRead(KEY);
  if(value!=raw){raw=value;changed=now;}
  if(now-changed>=25&&stable!=raw){stable=raw;if(stable==LOW&&!locked&&training>=0&&!trainingAuto&&!capturing)startCapture();else if(stable==HIGH){if(capturing&&!autoCapture)finishCapture();locked=false;}}
  now=millis();
  if(capturing&&wand::captureExpired(now,captureStart,lastSample)){badCapture=true;finishCapture();locked=true;}
  if(rebootAt&&int32_t(now-rebootAt)>=0)ESP.restart();
  if(recordPending&&!capturing&&int32_t(now-recordAt)>30000){recordPending=false;gate.reset(now);console.println("RECORD TIMEOUT click record to retry");}
  static String lastPhase;String phase=recordPhase();if(phase!=lastPhase){console.printf("RECORD phase=%s\n",phase.c_str());lastPhase=phase;}
  bool fault=!imuOK||now-lastSample>1000;
  if(feedback&&now-feedbackStart>900)feedback=0;
  bool green=false,red=false;
  if(fault)red=true;
  else if(!calibrated)red=(now/200)%2;
  else if(capturing)green=true;
  else if(recordPending)green=gate.armed?now%400<80:now%1000<150;
  else if(feedback>0)green=((now-feedbackStart)/150)%2==0;
  else if(feedback<0)red=((now-feedbackStart)/150)%2==0;
  else green=now%1000<50;
  digitalWrite(GREEN,green);digitalWrite(RED,red);
  if(now-lastReport>2000){lastReport=now;console.printf("HEALTH t=%lu samples=%lu I2C_ERRORS=%lu ready=%d heap=%u train=%d capturing=%d armed=%d linear=%.3f key=%d phase=%s\n",(unsigned long)(now/1000),(unsigned long)samples,(unsigned long)errors,calibrated&&!fault,ESP.getFreeHeap(),training,capturing,gate.armed,residualAcceleration,digitalRead(KEY),recordPhase());console.printf("LINK %s\n",wandLink.status().c_str());}
  delay(1);
}
