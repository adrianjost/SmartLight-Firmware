/**
Author: Adrian Jost
Version: 0.3.0
Date: 14 November 2019
**/
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <Hash.h>

// WIFI-Manager
#include <FS.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

// LED Strips
#include <Adafruit_NeoPixel.h>

// Which pin on the ESP8266 is connected to the NeoPixels?
#define NEO_PIN 2
#define NEO_BRIGHTNESS 100
#define NEO_PIXELS 100

// config storage
#define configFilePath "/config.json"
#define JSON_HOSTNAME "hn"
#define JSON_LAMP_TYPE "lt"


// current state
#define STATE_UNDEFINED 0
#define STATE_COLOR 1
#define STATE_GRADIENT 2

// Debug colors
#define BLACK RGB{0,0,0}
#define WHITE RGB{255,255,255}
#define RED RGB{55,0,0}
#define GREEN RGB{0,55,0}
#define BLUE RGB{0,0,55}
#define ORANGE RGB{55,55,0}
#define VIOLET RGB{55,0,55}

/*
API
===============================================================================
<color values are from 0-255>
Examples:
# 1 set color
send:
  {
    color: {
      r: 255,
      g: 100,
      b: 0
    }
  }

# 2 set gradient
send:
  {
    gradient: {
      colors: [
        {
          r: 255,
          g: 0,
          b: 0
        },
        {
          r: 0,
          g: 0,
          b: 255
        },
        {
          r: 255,
          g: 0,
          b: 0
        }
      ],
      transitionTimes: [0, 10000, 20000], // check 2.1
      loop: true // check 2.2
    }
  }

## 2.1 transitionTimes
a colorTimestamp n declars when the nth color should be fully visible in milliseconds (1s = 1000ms).
In the given example, we start with a red color at 0s and transition smoothly to blue.
After 10s we are showing a solid blue color and begin fading to the 3th color (red).
After 10 more seconds (20s in total) we are showing full red again.
! the first value must be 0 and the total number of transitionTimes must be equal to the number of gradient colors. !

## 2.2 loop
the loop option declares wheather the gradient should start again when reaching the last color.
We do not do any type of fading here!
To make this transition smooth your gradient should start and end with the same colors.
===============================================================================
*/


Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NEO_PIXELS, NEO_PIN, NEO_GRB + NEO_KHZ800);

WebSocketsServer webSocket = WebSocketsServer(80);

struct RGB {
  byte r;
  byte g;
  byte b;
};

struct floatRGB {
  float r;
  float g;
  float b;
};

RGB analogPinout = {1,2,3};


//*************************
// global State
//*************************

char lamptype[32] = "Analog"; // "NeoPixel", "Analog"
char hostname[32] = "A CHIP";

byte currentState = STATE_UNDEFINED;
bool hasNewValue = false;

// currentState == STATE_COLOR
RGB currentColor {0,0,0};

// currentState == STATE_GRADIENT
RGB *currentGradientColors; // create pointer where informations can be stored
unsigned long *currentGradientTimes; // create pointer where informations can be stored
byte numberOfSteps;
bool currentGradientLoop;

/**********************************
 INIT
**********************************/

void initStripNeoPixel(){
  pixels.begin(); // This initializes the NeoPixel library.
  pixels.setBrightness(NEO_BRIGHTNESS);
  pixels.show();
}
void initStripAnalog(){
  pinMode(analogPinout.r, OUTPUT);
  pinMode(analogPinout.g, OUTPUT);
  pinMode(analogPinout.b, OUTPUT);
}
void initStrip(){
  // set new color
  // "NeoPixel", "Analog"
  if(String(lamptype) == String("Analog")){
    initStripAnalog();
  }else{
    initStripNeoPixel();
  }
}

/**********************************
 SET COLOR
**********************************/

void setColorNeoPixel(RGB color){
  for (int i = 0; i < pixels.numPixels(); i++) {
    pixels.setPixelColor(i, pixels.Color(color.r, color.g, color.b));
  }
  pixels.show();
}
void setColorAnalog(RGB color){
  analogWrite(analogPinout.r, map(color.r,0,255,0,1024));
  analogWrite(analogPinout.g, map(color.g,0,255,0,1024));
  analogWrite(analogPinout.b, map(color.b,0,255,0,1024));
}
void setColor(RGB color){
  // set new color
  // "NeoPixel", "Analog"
  if(String(lamptype) == String("Analog")){
    setColorAnalog(color);
  }else{
    setColorNeoPixel(color);
  }
}

//*************************
// config manager
//*************************

WiFiManagerParameter setting_hostname(JSON_HOSTNAME, "Devicename: (e.g. <code>smartlight-kitchen</code>)", hostname, 32);
WiFiManagerParameter setting_lamptype(JSON_LAMP_TYPE, "Type of connected lamp:<br /><span>Options: <code>NeoPixel</code>, <code>Analog</code></span>", lamptype, 32);

void saveConfigCallback () {
  const size_t capacity = JSON_OBJECT_SIZE(2);
  DynamicJsonDocument doc(capacity);

  doc[JSON_HOSTNAME] = setting_hostname.getValue();
  doc[JSON_LAMP_TYPE] = setting_lamptype.getValue();

  File configFile = SPIFFS.open(configFilePath, "w");
  serializeJson(doc, configFile);
  configFile.close();

  setColor(GREEN);
  delay(1000);
  setColor(BLACK);
  ESP.restart();
}

void blink(RGB color, int speed){
  while(true){
    setColor(color);
    delay(speed);
    setColor(BLACK);
    delay(speed);
  }
}

void setupSpiffs(){
  // TODO remove color debug statements
  // initial values
  ("SmartLight-" + String(ESP.getChipId(), HEX)).toCharArray(hostname, 32);
  setColor(RED);
  delay(1000);
  if (SPIFFS.begin()) {
    setColor(ORANGE);
    delay(1000);
    if (SPIFFS.exists(configFilePath)) {
      setColor(GREEN);
      delay(1000);
      //file exists, reading and loading
      File configFile = SPIFFS.open(configFilePath, "r");
      if (configFile) {
        setColor(BLUE);
        delay(1000);
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

      setColor(RED);
      delay(1000);
        configFile.readBytes(buf.get(), size);

      setColor(ORANGE);
      delay(1000);
        const size_t capacity = JSON_OBJECT_SIZE(2) + 60;
        DynamicJsonDocument doc(capacity);
      setColor(GREEN);
      delay(1000);
        auto error = deserializeJson(doc, buf.get());
        if(error){
          blink(RED, 500);
          return;
        }
      setColor(BLUE);
      delay(1000);
        // copy from config to variable
        if(doc.containsKey(JSON_HOSTNAME)){
          setColor(RED);
          delay(500);
          strcpy(hostname, doc[JSON_HOSTNAME]);
          setColor(GREEN);
          delay(500);
        }
        if(doc.containsKey(JSON_LAMP_TYPE)){
          setColor(RED);
          delay(500);
          strcpy(lamptype, doc[JSON_LAMP_TYPE]);
          setColor(GREEN);
          delay(500);
        }
        blink(BLUE, 500);
      }
      blink(ORANGE, 500);
    }
  }
}

void setupWifi(){
  WiFiManager wm;
  wm.setDebugOutput(false);
  wm.setTimeout(300);
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.setHostname(hostname);

  wm.addParameter(&setting_hostname);
  wm.addParameter(&setting_lamptype);

  setColor(VIOLET);

  if(!wm.autoConnect("SmartLight Setup", "LightItUp")){
    setColor(RED);
    // shut down till the next reboot
    //ESP.deepSleep(86400000000); // 1 Day
    ESP.deepSleep(600000000); // 10 Minutes
    ESP.restart();
  }
}

//*************************
// gradient stuff
//*************************
bool gradientAcitve = false;
unsigned long lastStepTime;
unsigned long targetTime;
unsigned long timespan;
floatRGB currentGradientColor;
floatRGB dy;
byte gradientState;

RGB floatRGBtoRGB(floatRGB color){
  return RGB {
    (byte)color.r,
    (byte)color.g,
    (byte)color.b
  };
}

floatRGB RGBtofloatRGB(RGB color){
  return floatRGB {
    (float)color.r,
    (float)color.g,
    (float)color.b
  };
}

float gradientCalculateNewColorChannelValue(unsigned long dx, float dy, float color){
  return color + (dx * dy);
}

floatRGB gradientCalculateNewColor(unsigned long dx, floatRGB dy, floatRGB color){
  color = floatRGB {
    gradientCalculateNewColorChannelValue(dx, dy.r, color.r),
    gradientCalculateNewColorChannelValue(dx, dy.g, color.g),
    gradientCalculateNewColorChannelValue(dx, dy.b, color.b),
  };
  setColor(floatRGBtoRGB(color));
  return color;
}

//void fade(uint8_t num, unsigned long duration, RGB before, RGB after){
void gradientInitFade(unsigned long duration, RGB before, RGB after){
  // TODO FIX ? handle millis overflow
  lastStepTime = millis();
  targetTime = lastStepTime + duration;
  timespan = duration;

  dy = (floatRGB){
    (float)((after.r - before.r) / (float)timespan),
    (float)((after.g - before.g) / (float)timespan),
    (float)((after.b - before.b) / (float)timespan)
  };

  currentGradientColor = RGBtofloatRGB(before);

  gradientAcitve = true;
}

void gradientStep(){
  // breakpoint not reached -> continue fading
  if (lastStepTime < targetTime){ // TODO FIX ? handle millis overflow
    unsigned long currentTime = millis();
    unsigned long dx = currentTime - lastStepTime;
    lastStepTime = currentTime;
    currentGradientColor = gradientCalculateNewColor(dx, dy, currentGradientColor);
    return;
  }
  // transition done
  if(gradientState < (numberOfSteps - 1)){ // more transitions defined?
    // calculate next step
    gradientState++;
    RGB startColor = currentGradientColors[gradientState - 1];
    RGB targetColor = currentGradientColors[gradientState];
    unsigned long stopTimeStart = currentGradientTimes[gradientState - 1];
    unsigned long stopTimeTarget = currentGradientTimes[gradientState];
    unsigned long duration = stopTimeTarget - stopTimeStart;
    gradientInitFade(duration, startColor, targetColor);
    return;
  }
  // gradient should loop?
  if(currentGradientLoop){
    // restart gradient
    gradientState = 0;
    return;
  }
  // stop gradient
  gradientAcitve = false;
}

bool gradientLoop(bool restart){
  if(restart){
    gradientState = 0;
    gradientAcitve = true;
  }
  if(gradientAcitve){
    gradientStep();
    return true;
  }else{
    return false;
  }
}

//*************************
// websocket communication
//*************************

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
  switch(type) {
    case WStype_DISCONNECTED:{
        // DISCONNECTED
      }
      break;
    case WStype_CONNECTED:{
      // CONNECTED
      // webSocket.sendTXT(num,"{\"hostname\":\""+host+"\"}");
        webSocket.sendTXT(num, "CONNECTED");
      }
      break;
    case WStype_TEXT:{
        String text = String((char *) &payload[0]);
        if(text.startsWith("J")){
          // deprecated case
          // handle it by converting it to the new syntax (removing leading "J")
          text = (text.substring(text.indexOf("J")+1, text.length()));
        }

        const size_t capacity = JSON_ARRAY_SIZE(10) + JSON_ARRAY_SIZE(10) + JSON_OBJECT_SIZE(2) + 10*JSON_OBJECT_SIZE(3) + 150;
        DynamicJsonDocument tmpStateJson(capacity);
        auto error = deserializeJson(tmpStateJson, text);

        if (error) {
          // ERROR parsing State
          currentState = STATE_UNDEFINED;
          return;
        }
        if(tmpStateJson.containsKey("color")){
          currentColor = {
            tmpStateJson["color"]["1"],
            tmpStateJson["color"]["2"],
            tmpStateJson["color"]["3"]
          };
          currentState = STATE_COLOR;
          hasNewValue = true;
        }else if(tmpStateJson.containsKey("gradient")){
          gradientState = 0;
          targetTime = millis();
          free(currentGradientColors);
          free(currentGradientTimes);
          numberOfSteps = tmpStateJson["gradient"]["colors"].size();
          currentGradientColors = (RGB *) malloc(sizeof(RGB) * numberOfSteps);
          currentGradientTimes = (unsigned long *) malloc(sizeof(unsigned long) * numberOfSteps);

          currentGradientLoop = tmpStateJson["gradient"]["loop"];
          for (int i = 0; i < numberOfSteps; i++) {
            (currentGradientColors)[i] = RGB {
              tmpStateJson["gradient"]["colors"][i]["1"],
              tmpStateJson["gradient"]["colors"][i]["2"],
              tmpStateJson["gradient"]["colors"][i]["3"]
            };
            (currentGradientTimes)[i] = tmpStateJson["gradient"]["transitionTimes"][i];
          }
          currentState = STATE_GRADIENT;
          hasNewValue = true;
        }else{
          currentState = STATE_UNDEFINED;
        }
      }
      break;
  }
}

//*************************
// SETUP
//*************************

void setup() {
  
  initStrip();

  // blink(WHITE, 500);
  // TODO Spiffs before initStrip
  // setupSpiffs();

  setColor(BLUE);
  setupWifi();
  setColor(BLACK);

  currentGradientColors = (RGB *) malloc(sizeof(RGB));
  currentGradientTimes = (unsigned long *) malloc(sizeof(unsigned long));

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

//*************************
// LOOP
//*************************
void loop() {
  webSocket.loop(); // listen for websocket events
  if(currentState == STATE_COLOR){
    setColor(currentColor);
    hasNewValue = false;
    currentState = STATE_UNDEFINED;
  } else if(currentState == STATE_GRADIENT){
    if(!gradientLoop(hasNewValue)){
      currentState = STATE_UNDEFINED;
    }else{
        hasNewValue = false;
    }
  }
}
