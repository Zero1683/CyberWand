#pragma once
#include <Arduino.h>
#include <stdarg.h>
#include <hal/usb_serial_jtag_ll.h>
// Single-owner ring: telemetry never blocks the 104 Hz acquisition loop.
class BufferedConsole {
  char queue[8192]; size_t head=0,tail=0; uint32_t dropped=0; bool zlp=false;
public:
  size_t free() const {return (tail+sizeof(queue)-head-1)%sizeof(queue);}
  uint32_t drops()const{return dropped;}
  void print(const char* s){size_t n=strlen(s);if(n>free()){dropped++;return;}while(*s){queue[head]=*s++;head=(head+1)%sizeof(queue);}}
  void println(const char* s){printf("%s\n",s);}
  void printf(const char* fmt,...){char text[1024];va_list args;va_start(args,fmt);vsnprintf(text,sizeof(text),fmt,args);va_end(args);print(text);}
  void pump(){
    if(!usb_serial_jtag_ll_txfifo_writable())return;
    uint8_t packet[64];size_t n=std::min(size_t(64),(head+sizeof(queue)-tail)%sizeof(queue));
    for(size_t i=0;i<n;i++)packet[i]=queue[(tail+i)%sizeof(queue)];
    if(n){int sent=usb_serial_jtag_ll_write_txfifo(packet,n);tail=(tail+sent)%sizeof(queue);usb_serial_jtag_ll_txfifo_flush();zlp=sent==64;}
    else if(zlp){usb_serial_jtag_ll_txfifo_flush();zlp=false;}
  }
};
