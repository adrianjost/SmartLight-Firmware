/**
Author: Adrian Jost
Version: 0.3.0
Date: 22 December 2019
**/

// WiFiManager
// https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <WiFiManager.h>        // 1.0.0 - tzapu,tablatronix (GitHub Develop Branch c9665ad)
#include <FS.h>
#include <LittleFS.h>           // required esp8266-core >2.6.3

// Website Communication
#include <ESP8266WiFi.h>        // 1.0.0 - Ivan Grokhotkov
#include <ArduinoJson.h>        // 6.13.0 - Benoit Blanchon
#include <WebSocketsServer.h>   // 0.4.13 - Gil Maimon or 2.1.4 Markus Sattler (I am not sure which lib gets used)

// LED Strips
#include <Adafruit_NeoPixel.h>  // 1.3.1 Adafruit

// comment in for serial debugging
// #define DEBUG
#ifdef DEBUG
  #define DEBUG_SPEED 74880
#endif

// PIN DEFINITIONS
#define PIN_RESET 0 // comment out on boards without reset button
#define PIN_NEO 2
#define PIN_CH1 1
#define PIN_CH2 2
#define PIN_CH3 3

// NeoPixel settings
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
      1: 255,
      2: 100,
      3: 0
    }
  }

# 2 set gradient
send:
  {
    gradient: {
      colors: [
        {
          1: 255,
          2: 0,
          3: 0
        },
        {
          1: 0,
          2: 0,
          3: 255
        },
        {
          1: 255,
          2: 0,
          3: 0
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

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NEO_PIXELS, PIN_NEO, NEO_GRB + NEO_KHZ800);

FS* filesystem = &LittleFS;
WebSocketsServer webSocket = WebSocketsServer(80);

// max 25 gradient steps, 2226 Bytes, each timestamp max 7 digits, each color val max 255
const size_t maxPayloadSize = 2*JSON_ARRAY_SIZE(25) + 26*JSON_OBJECT_SIZE(3) + 200;
// 2 keys with each value max 32 chars long, 104 Bytes
const size_t maxStorageSize = JSON_OBJECT_SIZE(2) + 80;

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
  pinMode(PIN_CH1, OUTPUT);
  pinMode(PIN_CH2, OUTPUT);
  pinMode(PIN_CH3, OUTPUT);
}
void initStrip(){
  #ifdef DEBUG
    Serial.println("initStrip");
    Serial.println(String(lamptype));
    return;
  #endif

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
  analogWrite(PIN_CH1, map(color.r,0,255,0,1024));
  analogWrite(PIN_CH2, map(color.g,0,255,0,1024));
  analogWrite(PIN_CH3, map(color.b,0,255,0,1024));
}
void setColor(RGB color){
  #ifdef DEBUG
    Serial.print("setColor: ");
    Serial.print(color.r);
    Serial.print(", ");
    Serial.print(color.g);
    Serial.print(", ");
    Serial.print(color.b);
    Serial.println("");
    return;
  #endif

  // set new color
  // "NeoPixel", "Analog"
  if(String(lamptype) == String("Analog")){
    setColorAnalog(color);
  }else{
    setColorNeoPixel(color);
  }
}
void blink(RGB color, int speed){
  while(true){
    setColor(color);
    delay(speed);
    setColor(BLACK);
    delay(speed);
  }
}

//*************************
// config manager
//*************************

bool shouldSaveConfig = false;
WiFiManager *globalWiFiManager;

void saveConfigCallback () {
  #ifdef DEBUG
    Serial.println("shouldSaveConfig");
  #endif
  shouldSaveConfig = true;
  globalWiFiManager->stopConfigPortal();
}

void configModeCallback (WiFiManager *myWiFiManager) {
  globalWiFiManager = myWiFiManager;
  #ifdef DEBUG
    Serial.println("start config portal");
  #endif
  setColor(VIOLET);
}

void setupFilesystem(){
  #ifdef DEBUG
    Serial.println("setupFilesystem");
  #endif

  // initial values
  ("SmartLight-" + String(ESP.getChipId(), HEX)).toCharArray(hostname, 32);

  #ifdef DEBUG
    Serial.print("hostname: ");
    Serial.println(hostname);
    Serial.print("lamptype: ");
    Serial.println(lamptype);
  #endif

  #ifdef DEBUG
    Serial.println("exec filesystem->begin()");
  #endif
  filesystem->begin();
  #ifdef DEBUG
    Serial.println("filesystem->begin() executed");
  #endif

  if(!filesystem->exists(configFilePath)) {
    #ifdef DEBUG
      Serial.println("config file doesn't exist");
    #endif
    return;
  }
  #ifdef DEBUG
    Serial.println("configfile exists");
  #endif

  //file exists, reading and loading
  File configFile = filesystem->open(configFilePath, "r");
  if(!configFile) { return; }
  #ifdef DEBUG
    Serial.println("configfile read");
  #endif

  size_t size = configFile.size();
  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  DynamicJsonDocument doc(maxStorageSize);
  auto error = deserializeJson(doc, buf.get());
  if(error){ return; }
  configFile.close();

  #ifdef DEBUG
    Serial.println("configfile serialized");
  #endif

  // copy from config to variable
  if(doc.containsKey(JSON_HOSTNAME)){
    #ifdef DEBUG
      Serial.println("JSON_HOSTNAME key read");
    #endif
    strcpy(hostname, doc[JSON_HOSTNAME]);
    #ifdef DEBUG
      Serial.println("hostname updated");
    #endif
  }
  if(doc.containsKey(JSON_LAMP_TYPE)){
    #ifdef DEBUG
      Serial.println("JSON_LAMP_TYPE key read");
    #endif
    strcpy(lamptype, doc[JSON_LAMP_TYPE]);
    #ifdef DEBUG
      Serial.println("lamptype updated");
    #endif
  }
}

bool shouldEnterSetup(){
  #ifndef PIN_RESET
    return false;
  #else
    pinMode(PIN_RESET, INPUT);
    byte clickThreshould = 5;
    int timeSlot = 5000;
    byte readingsPerSecond = 10;
    byte click_count = 0;


    for(int i=0; i < (timeSlot / readingsPerSecond / 10); i++){
      byte buttonState = digitalRead(PIN_RESET);
      if(buttonState == LOW){
        click_count++;
        if(click_count >= clickThreshould){
          setColor(VIOLET);
          pinMode(PIN_RESET, OUTPUT);
          digitalWrite(PIN_RESET, HIGH);
          return true;
        }
      } else {
        click_count = 0;
      }
      delay(1000 / readingsPerSecond);
    }
    return false;
  #endif
}

void setupWifi(){
  WiFiManager wm;
  wm.setDebugOutput(false);
  // close setup after 5min
  wm.setTimeout(300);
  // set dark theme
  wm.setClass("invert");

  wm.setSaveParamsCallback(saveConfigCallback);
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.setAPCallback(configModeCallback);

  wm.setHostname(hostname);

  WiFiManagerParameter setting_hostname(JSON_HOSTNAME, "Devicename: (e.g. <code>smartlight-kitchen</code>)", hostname, 32);
  WiFiManagerParameter setting_lamptype(JSON_LAMP_TYPE, "Type of connected lamp:<br /><span>Options: <code>NeoPixel</code>, <code>Analog</code></span>", lamptype, 32);
  wm.addParameter(&setting_hostname);
  wm.addParameter(&setting_lamptype);

  setColor(GREEN);

  bool forceSetup = shouldEnterSetup();
  bool setup = forceSetup
    ? wm.startConfigPortal("SmartLight Setup", "LightItUp")
    : wm.autoConnect("SmartLight Setup", "LightItUp");

  if (shouldSaveConfig) {
    #ifdef DEBUG
      Serial.println("write config to filesystem");
    #endif
    DynamicJsonDocument doc(maxStorageSize);

    doc[JSON_HOSTNAME] = setting_hostname.getValue();
    doc[JSON_LAMP_TYPE] = setting_lamptype.getValue();

    File configFile = filesystem->open(configFilePath, "w");
    serializeJson(doc, configFile);
    configFile.close();

    #ifdef DEBUG
      Serial.println("config written to filesystem");
    #endif

    setColor(GREEN);
    delay(1000);
    setColor(BLACK);
    ESP.restart();
  }

  if(!setup){
    setColor(RED);
    // shut down till the next reboot
    // ESP.deepSleep(86400000000); // 1 Day
    ESP.deepSleep(600000000); // 10 Minutes
    ESP.restart();
  }

  if(forceSetup){
    ESP.restart();
  }
}

//*************************
// gradient stuff
//*************************
bool gradientAcitve = false;
unsigned long stepStartTime;
unsigned long lastStepTime;
unsigned long targetTime;
unsigned long currentStepDuration;
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
  stepStartTime = millis();
  currentStepDuration = duration;
  lastStepTime = stepStartTime;

  dy = (floatRGB){
    (float)((after.r - before.r) / (float)duration),
    (float)((after.g - before.g) / (float)duration),
    (float)((after.b - before.b) / (float)duration)
  };

  currentGradientColor = RGBtofloatRGB(before);

  gradientAcitve = true;
}

void gradientStep(){
  // breakpoint not reached -> continue fading
  unsigned long currentTime = millis();
  if((unsigned long)(currentTime - stepStartTime) < currentStepDuration) {
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
        DynamicJsonDocument tmpStateJson(maxPayloadSize);
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
          currentStepDuration = 0;
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
  #ifdef DEBUG
    Serial.begin(DEBUG_SPEED);
    Serial.print("\n");
    Serial.setDebugOutput(true);
    Serial.println("STARTED IN DEBUG MODE");
  #endif

  setupFilesystem();

  #ifdef DEBUG
    Serial.println("setupFilesystem finished");
  #endif

  initStrip();

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
