#include <ArduinoJson.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <Hash.h>
WebSocketsServer webSocket = WebSocketsServer(9000, "");
const char* ssid     = "{{SSID}}";
const char* password = "{{PASSWORD}}";
String host = "SmartLight-{{ID}}";

struct RGB {
  byte r;
  byte g;
  byte b;
};

RGB color = { 0,0,0 };

void set_color(){
  // set new color
  analogWrite(0,map(color.g,0,255,0,1024));
  analogWrite(1,map(color.b,0,255,0,1024));
  analogWrite(2,map(color.r,0,255,0,1024));
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
  switch(type) {
    case WStype_DISCONNECTED:{
        //color={0,0,0}; 
        //set_color();
        /*RGB oldcol = color;
        color={55,0,0}; 
        set_color();
        delay(1000);
        color=oldcol; 
        set_color();*/
      }
      break;
    case WStype_CONNECTED:{
        /*IPAddress ip = webSocket.remoteIP(num);
        RGB oldcol = color;
        color={0,55,0}; 
        set_color();
        delay(1000);
        color=oldcol; 
        set_color();
        webSocket.sendTXT(num,"{\"hostname\":\""+host+"\"}");
        */
      }
      break;
    case WStype_TEXT:{
        String text = String((char *) &payload[0]);
        if(text=="RESET"){
          color = {0,0,0}; 
          set_color();            
        }
        else if(text.startsWith("J")){
          StaticJsonBuffer<100> jsonBuffer;
          String raw = (text.substring(text.indexOf("J")+1,text.length())); 
          JsonObject& values = jsonBuffer.parseObject(raw);
          if (!values.success()) {
            color = {55,55,0}; 
            set_color();
          }else{
            color = {values["r"],values["g"],values["b"]}; 
            set_color();
          }
        }
      }
      break;
    case WStype_BIN:{
        //hexdump(payload, lenght);
        // echo data back to browser
        //webSocket.sendBIN(num, payload, lenght);
      }
      break;
  }
}


void setup() {
  pinMode(0,OUTPUT);
  pinMode(1,OUTPUT);
  pinMode(2,OUTPUT);
  
  WiFi.begin(ssid, password);
  WiFi.hostname(host);
  color={0,0,55}; 
  set_color();
  while(WiFi.status() != WL_CONNECTED) {
    delay(100);
  }
  color={0,0,0}; 
  set_color();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  webSocket.loop();
}
