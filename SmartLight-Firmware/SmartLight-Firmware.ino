/**
Author: Adrian Jost
Version: 3.1.0
Date: 7 June 2020
**/

// PIN DEFINITIONS
#define PIN_RESET 0 // comment out on boards without FLASH-button
#define PIN_INPUT 2
// TODO: remove all NEO-Pixel code if not defined
// #define PIN_NEO 1
#define PIN_CH1 1
// #define PIN_CH2 3
#define PIN_CH3 3

// NeoPixel settings
#define NEO_BRIGHTNESS 100
#define NEO_PIXELS 100

// config storage
#define configFilePath "/config.json"
#define JSON_HOSTNAME "hn"
#define JSON_LAMP_TYPE "lt"

// current state
#define STATE_OFF 0
#define STATE_UNDEFINED 1
#define STATE_COLOR 2
#define STATE_GRADIENT 3
#define STATE_TIME 4

// Debug colors
#define BLACK RGB{0,0,0}
#define WHITE RGB{20,20,20}
#define RED RGB{20,0,0}
#define GREEN RGB{0,20,0}
#define BLUE RGB{0,0,20}
#define ORANGE RGB{20,20,0}
#define VIOLET RGB{20,0,20}

// button control
#define TIMEOUT 500
#define TIMEOUT_INFINITY 60000 // 1min
#define BRIGHTNESS_MIN 0
#define BRIGHTNESS_MAX 255
#define BRIGHNESS_STEP 1
#define BRIGHNESS_STEP_DURATION 60
#define HUE_MIN 0
#define HUE_MAX 1
#define HUE_STEP 0.01
#define HUE_STEP_DURATION 120

// Durations in ms
#define DURATION_MINUTE 60000

// WiFiManager
// https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <WiFiManager.h>        // 1.0.0 - tzapu,tablatronix (GitHub Develop Branch c9665ad)
#include <FS.h>
#include <LittleFS.h>           // requires esp8266-core >2.6.3

// Website Communication
#include <ESP8266WiFi.h>        // 1.0.0 - Ivan Grokhotkov
#include <ArduinoJson.h>        // 6.16.1 - Benoit Blanchon
#include <WebSocketsServer.h>   // 2.1.4 Markus Sattler

#include <ESP8266WebServer.h>   //

// OTA Updates
#include <ArduinoOTA.h>

// LED Strips
#ifdef PIN_NEO
#include <Adafruit_NeoPixel.h>  // 1.3.1 Adafruit
#endif

// Time Keeping
#include <NTPClient.h> // 3.2.0 Fabrice Weinberg
#include <WiFiUdp.h>

// comment in for serial debugging
// #define DEBUG
#ifdef DEBUG
  #define DEBUG_SPEED 115200
#endif

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

WiFiManager wm;

#ifdef PIN_NEO
  Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NEO_PIXELS, PIN_NEO, NEO_GRB + NEO_KHZ800);
#endif

FS* filesystem = &LittleFS;
ESP8266WebServer server(81);
WebSocketsServer webSocket = WebSocketsServer(80);

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0);

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
char wifiPassword[32] = "";


byte currentState = STATE_OFF;
bool hasNewValue = false;

// currentState == STATE_COLOR
RGB currentColor {0,0,0};

// currentState == STATE_GRADIENT
RGB *currentGradientColors; // create pointer where informations can be stored
unsigned long *currentGradientTimes; // create pointer where informations can be stored
byte numberOfSteps;
bool currentGradientLoop;

// currentState == STATE_UNDEFINED || currentState == STATE_TIME
byte brightness = 0;
float hue = 0.5;

/**********************************
 INIT
**********************************/

void initStripNeoPixel(){
  #ifdef PIN_NEO
    pixels.begin(); // This initializes the NeoPixel library.
    pixels.setBrightness(NEO_BRIGHTNESS);
    pixels.show();
  #endif
}
void initStripAnalog(){
  #ifdef PIN_CH1
    pinMode(PIN_CH1, OUTPUT);
  #endif
  #ifdef PIN_CH2
    pinMode(PIN_CH2, OUTPUT);
  #endif
  #ifdef PIN_CH3
    pinMode(PIN_CH3, OUTPUT);
  #endif
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
  #ifdef PIN_NEO
    for (int i = 0; i < pixels.numPixels(); i++) {
      pixels.setPixelColor(i, pixels.Color(color.r, color.g, color.b));
    }
    pixels.show();
  #endif
}
void setColorAnalog(RGB color){
  #ifdef PIN_CH1
    analogWrite(PIN_CH1, map(color.r,0,255,0,1024));
  #endif
  #ifdef PIN_CH2
    analogWrite(PIN_CH2, map(color.g,0,255,0,1024));
  #endif
  #ifdef PIN_CH3
    analogWrite(PIN_CH3, map(color.b,0,255,0,1024));
  #endif
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
    ESP.deepSleep(300000000); // 5 Minutes
    ESP.restart();
  }

  if(forceSetup){
    ESP.restart();
  }

  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
}

void setupOTAUpdate(){
  wm.getWiFiPass().toCharArray(wifiPassword, 32);
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.setPassword(wifiPassword);

  #ifdef DEBUG
    ArduinoOTA.onStart([]() {
      Serial.println("Start");
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
  #endif
  ArduinoOTA.begin();
  #ifdef DEBUG
    Serial.println("OTA ready");
  #endif
}

//*************************
// gradient stuff
//*************************
bool gradientActive = false;
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
  currentColor = floatRGBtoRGB(color);
  setColor(currentColor);
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

  gradientActive = true;
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
  gradientActive = false;
}

bool gradientLoop(bool restart){
  if(restart){
    gradientState = 0;
    gradientActive = true;
  }
  if(gradientActive){
    gradientStep();
    return true;
  }else{
    return false;
  }
}

//*************************
// webserver communication
//*************************

void handleRoot() {
  server.send(200, "text/plain", "Available routes:\nGET /color");   // Send HTTP status 200 (Ok) and send some text to the browser/client
}

void handleNotFound(){
  server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}

void handleColor(){
    server.send(200, "text/json", "{\"color\":{\"1\":\"" + String(currentColor.r) + "\",\"2\":\"" + String(currentColor.g) + "\",\"3\":\"" + String(currentColor.b) + "\"}}");
}

void setupServer(){
  server.on("/", handleRoot);
  server.on("/color", handleColor);
  server.onNotFound(handleNotFound);

  server.begin();
}


//*************************
// websocket communication
//*************************

void broadcastCurrentColor() {
  webSocket.broadcastTXT("{\"color\":{\"1\":" + String(currentColor.r) + ",\"2\":" + String(currentColor.g) + ",\"3\":" + String(currentColor.b) + "}}");
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
  switch(type) {
    case WStype_DISCONNECTED:{
        // DISCONNECTED
      }
      break;
    case WStype_CONNECTED:{
        // CONNECTED
        broadcastCurrentColor();
      }
      break;
    case WStype_TEXT:{
        String text = String((char *) &payload[0]);
        DynamicJsonDocument tmpStateJson(maxPayloadSize);
        auto error = deserializeJson(tmpStateJson, text);

        if (error) {
          // ERROR parsing State
          currentState = STATE_UNDEFINED;
          webSocket.sendTXT(num, "{\"status\":\"Error\",\"data\":\"Failed to parse payload\"}");
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
          broadcastCurrentColor();
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
          webSocket.sendTXT(num, "{\"status\":\"OK\"}");
        }else{
          currentState = STATE_UNDEFINED;
          webSocket.sendTXT(num, "{\"status\":\"Error\",\"data\":\"Unknown Payload\"}");
        }
      }
      break;
  }
}

void setupWebsocket(){
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

//*************************
// button control
//*************************

void updateLED() {
  float ww = hue < 0.5 ? 1 : (2 - (2 * hue));
  float cw = hue < 0.5 ? (hue * 2) : 1;
  currentColor = floatRGBtoRGB({
    constrain(brightness * ww, BRIGHTNESS_MIN, BRIGHTNESS_MAX),
    0,
    constrain(brightness * cw, BRIGHTNESS_MIN, BRIGHTNESS_MAX)
  });
  setColor(currentColor);
}

bool isBtn(bool state = true, unsigned long debounceDuration = 50) {
  // debounce
  unsigned long touchStart = millis();
  unsigned long match = 0;
  unsigned long noMatch = 0;
  do {
    if (digitalRead(PIN_INPUT) != state) {
      match += 1;
    }else{
      noMatch += 1;
    }
    // TODO: use min method instead of ternariy operator
    delay(debounceDuration < 5 ? debounceDuration : 5);
  } while(millis() - touchStart < debounceDuration);
  return (match >= noMatch);
}

bool waitForBtn(int ms, bool state = true) {
  if(ms == 0){
    return isBtn(state, 0);
  }
  unsigned long start = millis();
  do {
    if (isBtn(state)) {
      return true;
    }
  } while (millis() - start < ms);
  return false;
}

float getHue(RGB color) {
  floatRGB c = RGBtofloatRGB(color);
  float warm = c.r;
  float cold = c.b;
  if(warm == cold){
    return 0.5;
  }
  if(warm == 0){
    return 1;
  }
  if(cold == 0){
    return 0;
  }
  if(warm >= cold){
    return (cold / (2 * warm));
  }else {
    return 1 - (warm / (2 * cold));
  }
}

float getBrightness(RGB color) {
  return max(color.r, color.b);
}

void handleButton(){
  if (waitForBtn(0, true)) {
    if (waitForBtn(TIMEOUT, false)) {
      if (waitForBtn(TIMEOUT, true)) {
        if (waitForBtn(TIMEOUT, false)) {
          if (waitForBtn(TIMEOUT, true)) {
            if (waitForBtn(TIMEOUT, false)) {
              if (waitForBtn(TIMEOUT, true) && !waitForBtn(TIMEOUT, false)) {
                // tap tap tap hold
                currentState = STATE_UNDEFINED;
                // cycle hue +
                while (hue + HUE_STEP <= HUE_MAX && isBtn(true, HUE_STEP_DURATION)) {
                  hue += HUE_STEP;
                  updateLED();
                }
              }
            } else {
              // tap tap hold
              currentState = STATE_UNDEFINED;
              // cycle hue -
              while (hue - HUE_STEP >= HUE_MIN && isBtn(true, HUE_STEP_DURATION)) {
                hue -= HUE_STEP;
                updateLED();
              }
            }
          } else {
            // tap tap
          }
        } else {
          // tap, hold
          currentState = STATE_UNDEFINED;
          // increase brightness
          while (brightness + BRIGHNESS_STEP <= BRIGHTNESS_MAX && isBtn(true, BRIGHNESS_STEP_DURATION)) {
            brightness += BRIGHNESS_STEP;
            updateLED();
          }
        }
      } else {
        // tap
        if(currentState == STATE_OFF){
          // on
          currentState = STATE_TIME;
        }else{
          // off
          brightness = BRIGHTNESS_MIN;
          updateLED();
          currentState = STATE_OFF;
        }
      }
    } else {
      // hold
      currentState = STATE_UNDEFINED;
      // reduce brightness
      while (brightness - BRIGHNESS_STEP >= BRIGHTNESS_MIN && isBtn(true, BRIGHNESS_STEP_DURATION)) {
        brightness -= BRIGHNESS_STEP;
        updateLED();
      }
    }
    waitForBtn(TIMEOUT_INFINITY, false);
  }
}

//*************************
// time control
//*************************
#define TIMEZONE_OFFSET 2 // TODO: values should always be saved in UTC
// 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23
byte time_brightness[24] = {1, 1, 3, 5, 13, 51, 128, 204, 230, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 130, 40, 20, 5, 1};
float time_hue[24] = {0.01, 0.01, 0.01, 0.02, 0.05, 0.2, 0.5, 0.8, 0.9, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0.8, 0.5, 0.3, 0.1, 0.05, 0.01};

#define STEP_PRECISION 10000.0

unsigned long lastTimeUpdate = 0;
unsigned long lastTimeSend = 0;
void setByTime() {
  if(millis() - lastTimeUpdate > DURATION_MINUTE * 5){
    timeClient.update();
    lastTimeUpdate = millis();
  }
  byte minutes = timeClient.getMinutes() % 60;
  byte hour = timeClient.getHours() % 24;
  brightness = map(minutes, 0, 60, time_brightness[(hour + TIMEZONE_OFFSET) % 24], time_brightness[(hour + TIMEZONE_OFFSET + 1) % 24]);
  hue = map(
      minutes, 0, 60, 
      (unsigned int)(time_hue[(hour + TIMEZONE_OFFSET) % 24] * STEP_PRECISION),
      (unsigned int)(time_hue[(hour + TIMEZONE_OFFSET + 1) % 24] * STEP_PRECISION)
    ) / STEP_PRECISION;
  if(millis() - lastTimeSend > 1000){
    webSocket.broadcastTXT("MINUTES: " + String(minutes) + " HOUR: " + String(hour));
    webSocket.broadcastTXT("BRIGHTNESS: " + String(brightness) + " HUE: " + String(hue));
    broadcastCurrentColor();
    lastTimeSend = millis();
  }
  updateLED();
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

  #ifdef PIN_INPUT
    pinMode(PIN_INPUT, INPUT);
  #endif

  setupFilesystem();

  #ifdef DEBUG
    Serial.println("setupFilesystem finished");
  #endif

  initStrip();

  setColor(BLUE);
  setupWifi();
  setColor(GREEN);
  setupOTAUpdate();
  setColor(BLACK);

  currentGradientColors = (RGB *) malloc(sizeof(RGB));
  currentGradientTimes = (unsigned long *) malloc(sizeof(unsigned long));

  setupServer();
  setupWebsocket();

  timeClient.begin();
  timeClient.update();
}

//*************************
// LOOP
//*************************

void loop() {
  ArduinoOTA.handle(); // listen for OTA Updates
  webSocket.loop(); // listen for websocket events
  server.handleClient(); // listen for HTTP Requests
  #ifdef PIN_INPUT
    handleButton(); // listen for hardware inputs
  #endif
  switch (currentState) {
    case STATE_COLOR: {
        setColor(currentColor);
        hasNewValue = false;
        currentState = STATE_UNDEFINED;
        brightness = getBrightness(currentColor);
        hue = getHue(currentColor);
      }
      break;
    case STATE_GRADIENT: {
        if(!gradientLoop(hasNewValue)){
          currentState = STATE_UNDEFINED;
        }else{
          hasNewValue = false;
        }
        brightness = getBrightness(currentColor);
        hue = getHue(currentColor);
      }
      break;
    case STATE_TIME: {
        setByTime();
      }
      break;
    case STATE_UNDEFINED: {
        if(currentColor.r == 0 && currentColor.g == 0&& currentColor.b == 0){
          currentState = STATE_OFF;
        }
      }
      break;
  }
}
