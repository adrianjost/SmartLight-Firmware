/**
Author: Adrian Jost
Version: 4.0.0
Date: 7 June 2020
**/

// PIN DEFINITIONS
#define PIN_RESET 0 // comment out on boards without FLASH-button
#define PIN_INPUT 2
#define PIN_CH1 1
#define PIN_CH2 3

// config storage
#define PATH_CONFIG_WIFI "/config.json"
#define JSON_HOSTNAME "hn"
#define JSON_LAMP_TYPE "lt"

#define PATH_CONFIG_TIME "/time.json"
#define JSON_TIME_BRIGHTNESS "tb"
#define JSON_TIME_HUE "th"

// Debug colors
#define OFF Channels{0,0}
#define ON Channels{20,20}

// current state
#define STATE_OFF 0
#define STATE_UNDEFINED 1
#define STATE_COLOR 2
#define STATE_TIME 3

// button control
#define TIMEOUT 500
#define TIMEOUT_INFINITY 60000 // 1min
#define BRIGHTNESS_MIN 0
#define BRIGHTNESS_MAX 255
#define BRIGHNESS_STEP 1
#define BRIGHNESS_STEP_DURATION 90
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

// OTA Updates
#include <ArduinoOTA.h>

// Time Keeping
#include <NTPClient.h> // 3.2.0 Fabrice Weinberg
#include <WiFiUdp.h>

// comment in for serial debugging
// #define DEBUG
#ifdef DEBUG
  #define DEBUG_SPEED 115200
#endif

WiFiManager wm;

FS* filesystem = &LittleFS;
WebSocketsServer webSocket = WebSocketsServer(80);

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0);

// JSON sizes https://arduinojson.org/v6/assistant/
// { "color": { "1": 255, "2": 255, "3": 255 } }
const size_t maxPayloadSize = JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(3) + 20;

// { "JSON_HOSTNAME": "abcdef", "JSON_LAMP_TYPE": "test" }
const size_t maxWifiConfigSize = JSON_OBJECT_SIZE(2) + 80;

// { "JSON_TIME_BRIGHTNESS": 24[255], "JSON_TIME_HUE": 24[100] }
const size_t maxTimeConfigSize = 2*JSON_ARRAY_SIZE(24) + JSON_OBJECT_SIZE(2) + 30;

struct Channels {
  byte a;
  byte b;
};

struct FloatChannels {
  float a;
  float b;
};

//*************************
// global State
//*************************

char hostname[32] = "A CHIP";
char wifiPassword[32] = "";


byte currentState = STATE_OFF;
bool hasNewValue = false;

// currentState == STATE_COLOR
Channels currentOutput {0,0};

// currentState == STATE_UNDEFINED || currentState == STATE_TIME
byte brightness = 0;
float hue = 0.5;
#define TIMEZONE_OFFSET 1 // TODO: values should always be saved in UTC
// 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23
byte time_brightness[24] = {1, 1, 3, 5, 13, 51, 128, 204, 230, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 130, 40, 20, 5, 1};
float time_hue[24] = {0.01, 0.01, 0.01, 0.02, 0.05, 0.2, 0.5, 0.8, 0.9, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0.8, 0.5, 0.3, 0.1, 0.05, 0.01};

/**********************************
 INIT
**********************************/

void initStrip(){
  #ifdef DEBUG
    Serial.println("initStrip");
  #endif

  #ifdef PIN_CH1
    pinMode(PIN_CH1, OUTPUT);
  #endif
  #ifdef PIN_CH2
    pinMode(PIN_CH2, OUTPUT);
  #endif
}

/**********************************
 SET COLOR
**********************************/

void setOutput(Channels ch){
  #ifdef DEBUG
    Serial.print("setOutput: ");
    Serial.print(ch.a);
    Serial.print(", ");
    Serial.print(ch.b);
    Serial.println("");
    return;
  #endif

  #ifdef PIN_CH1
    analogWrite(PIN_CH1, map(ch.a,0,255,0,1024));
  #endif
  #ifdef PIN_CH2
    analogWrite(PIN_CH2, map(ch.b,0,255,0,1024));
  #endif
}

void blink(Channels ch, byte num, int blinkDuration){
  for (byte i = 0; i <= num; i++) {
    setOutput(ch);
    delay(blinkDuration);
    setOutput(OFF);
    delay(blinkDuration);
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
  blink(ON, 3, 200);
}

void setupWifiConfig() {
  #ifdef DEBUG
    Serial.println("setupFilesystem");
  #endif

  // initial values
  ("SmartLight-" + String(ESP.getChipId(), HEX)).toCharArray(hostname, 32);

  #ifdef DEBUG
    Serial.print("hostname: ");
    Serial.println(hostname);
  #endif

  #ifdef DEBUG
    Serial.println("exec filesystem->begin()");
  #endif
  filesystem->begin();
  #ifdef DEBUG
    Serial.println("filesystem->begin() executed");
  #endif

  if(!filesystem->exists(PATH_CONFIG_WIFI)) {
    #ifdef DEBUG
      Serial.println("config file doesn't exist");
    #endif
    return;
  }
  #ifdef DEBUG
    Serial.println("configfile exists");
  #endif

  //file exists, reading and loading
  File configFile = filesystem->open(PATH_CONFIG_WIFI, "r");
  if(!configFile) { return; }
  #ifdef DEBUG
    Serial.println("configfile read");
  #endif

  size_t size = configFile.size();
  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  DynamicJsonDocument doc(maxWifiConfigSize);
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
}

void setupTimeConfig() {
 #ifdef DEBUG
    Serial.println("setupFilesystem");
  #endif

  #ifdef DEBUG
    Serial.println("exec filesystem->begin()");
  #endif
  filesystem->begin();
  #ifdef DEBUG
    Serial.println("filesystem->begin() executed");
  #endif

  if(!filesystem->exists(PATH_CONFIG_TIME)) {
    #ifdef DEBUG
      Serial.println("config file doesn't exist");
    #endif
    return;
  }
  #ifdef DEBUG
    Serial.println("configfile exists");
  #endif

  //file exists, reading and loading
  File configFile = filesystem->open(PATH_CONFIG_TIME, "r");
  if(!configFile) { return; }
  #ifdef DEBUG
    Serial.println("configfile read");
  #endif

  size_t size = configFile.size();
  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  DynamicJsonDocument doc(maxTimeConfigSize);
  auto error = deserializeJson(doc, buf.get());
  if(error){ return; }
  configFile.close();

  #ifdef DEBUG
    Serial.println("configfile serialized");
  #endif

  // copy from config to variable
  if(doc.containsKey(JSON_TIME_BRIGHTNESS)){
    #ifdef DEBUG
      Serial.println("JSON_TIME_BRIGHTNESS key read");
    #endif
    byte numberOfSteps = 24;
    for (byte i = 0; i < numberOfSteps; i++) {
      time_brightness[i] = (byte) doc[JSON_TIME_BRIGHTNESS][i];
    }
    #ifdef DEBUG
      Serial.println("time brigthness updated");
    #endif
  }
  if(doc.containsKey(JSON_TIME_HUE)){
    #ifdef DEBUG
      Serial.println("JSON_TIME_HUE key read");
    #endif
    byte numberOfSteps = 24;
    for (byte i = 0; i < numberOfSteps; i++) {
      time_hue[i] = (float) ((float)doc[JSON_TIME_HUE][i] / 100.0);
    }
    #ifdef DEBUG
      Serial.println("time hue updated");
    #endif
  }
}

void setupFilesystem(){
  setupWifiConfig();
  setupTimeConfig();
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
  wm.addParameter(&setting_hostname);

  bool forceSetup = shouldEnterSetup();
  bool setup = forceSetup
    ? wm.startConfigPortal("SmartLight Setup", "LightItUp")
    : wm.autoConnect("SmartLight Setup", "LightItUp");

  if (shouldSaveConfig) {
    #ifdef DEBUG
      Serial.println("write config to filesystem");
    #endif
    DynamicJsonDocument doc(maxWifiConfigSize);

    doc[JSON_HOSTNAME] = setting_hostname.getValue();

    File configFile = filesystem->open(PATH_CONFIG_WIFI, "w");
    serializeJson(doc, configFile);
    configFile.close();

    #ifdef DEBUG
      Serial.println("config written to filesystem");
    #endif

    ESP.restart();
  }

  if(!setup){
    blink(ON, 10, 200);
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
// websocket communication
//*************************

void broadcastCurrentColor() {
  webSocket.broadcastTXT("{\"color\":{\"1\":" + String(currentOutput.a) + ",\"2\":" + String(currentOutput.b) + "}}");
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
          webSocket.sendTXT(num, "{\"status\":\"Error\",\"data\":\"Failed to parse payload\"}");
          return;
        }
        if(tmpStateJson.containsKey("color")){
          currentOutput = {
            tmpStateJson["color"]["1"],
            tmpStateJson["color"]["2"]
          };
          currentState = STATE_COLOR;
          hasNewValue = true;
          broadcastCurrentColor();
        }else{
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

Channels floatChtoCh(FloatChannels color){
  return Channels {
    (byte)color.a,
    (byte)color.b,
  };
}

FloatChannels chtoFloatCh(Channels color){
  return FloatChannels {
    (float)color.a,
    (float)color.b,
  };
}

void updateLED() {
  float ww = hue < 0.5 ? 1 : (2 - (2 * hue));
  float cw = hue < 0.5 ? (hue * 2) : 1;
  currentOutput = floatChtoCh({
    constrain(brightness * ww, BRIGHTNESS_MIN, BRIGHTNESS_MAX),
    constrain(brightness * cw, BRIGHTNESS_MIN, BRIGHTNESS_MAX)
  });
  setOutput(currentOutput);
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

float getHue(Channels ch) {
  FloatChannels c = chtoFloatCh(ch);
  float warm = c.a;
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

float getBrightness(Channels ch) {
  return max(ch.a, ch.b);
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

  blink(ON, 1, 200);
  setupWifi();
  blink(ON, 2, 200);
  setupOTAUpdate();

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
  #ifdef PIN_INPUT
    handleButton(); // listen for hardware inputs
  #endif
  switch (currentState) {
    case STATE_COLOR: {
        setOutput(currentOutput);
        hasNewValue = false;
        currentState = STATE_UNDEFINED;
        brightness = getBrightness(currentOutput);
        hue = getHue(currentOutput);
      }
      break;
    case STATE_TIME: {
        setByTime();
      }
      break;
  }
}
