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
#define PATH_CONFIG_TIME "/time.json"
#define JSON_TIME_BRIGHTNESS "tb"
#define JSON_TIME_HUE "th"

// Debug colors
#define OFF Channels{0,0}
#define ON Channels{20,20}

// current state
#define STATE_OFF 0
#define STATE_MANUAL 1
#define STATE_TIME 2

// button control
#define SHORT_PRESS_TIMEOUT 750
#define TIMEOUT_INFINITY 60000 // 1min
#define BRIGHTNESS_MIN 0
#define BRIGHTNESS_MAX 255
#define BRIGHNESS_STEP 1
#define BRIGHNESS_STEP_DURATION 15
#define HUE_MIN 0
#define HUE_MAX 1
#define HUE_STEP 0.01
#define HUE_STEP_DURATION 70

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
// { "hostname": "abcdef" }
const size_t maxWifiConfigSize = JSON_OBJECT_SIZE(2) + 80;
const size_t maxTimeConfigSize = 2*JSON_ARRAY_SIZE(24) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + 140;
// WebSocket Payload (max value is time light configuration)
const size_t maxPayloadSize = maxTimeConfigSize;


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

// currentState == STATE_MANUAL
Channels lastOutput {0,0};
Channels currentOutput {0,0};

// currentState == STATE_TIME
byte brightness = 0;
float hue = 0.5;
// 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23
byte time_brightness[24] = {1,1,3,5,13,51,128,204,230,255,255,255,255,255,255,255,255,255,180,100,40,25,15,3};
byte time_hue[24] = {0,0,0,2,5,10,20,40,50,60,70,70,70,70,70,70,70,50,30,20,0,0,0,0};
int time_utc_offset = 0;
char* time_server = (char *)"pool.ntp.org";

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

void blink(Channels ch, byte numberOfBlinks, int blinkDuration){
  setOutput(OFF);
  delay(blinkDuration);
  for (byte i = 0; i < numberOfBlinks; i++) {
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
  if(doc.containsKey("hostname")){
    #ifdef DEBUG
      Serial.println("hostname key read");
    #endif
    strcpy(hostname, doc["hostname"]);
    #ifdef DEBUG
      Serial.println("hostname updated");
    #endif
  }
}

void setupTimeConfig() {
  filesystem->begin();
  if(!filesystem->exists(PATH_CONFIG_TIME)) {
    #ifdef DEBUG
      Serial.println("config file doesn't exist");
    #endif
    return;
  }
  //file exists, reading and loading
  File configFile = filesystem->open(PATH_CONFIG_TIME, "r");
  if(!configFile) {
    #ifdef DEBUG
      Serial.println("config file is empty");
    #endif
    return;
  }

  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  DynamicJsonDocument doc(maxTimeConfigSize);
  auto error = deserializeJson(doc, buf.get());
  if(error){
    #ifdef DEBUG
      Serial.println("failed to parse config file");
    #endif
    return; }
  configFile.close();

  // copy from config to variable
  if(doc.containsKey("brightness")){
    byte numberOfSteps = 24;
    for (byte i = 0; i < numberOfSteps; i++) {
      time_brightness[i] = (byte) doc["brightness"][i];
    }
  }
  if(doc.containsKey("ratio")){
    byte numberOfSteps = 24;
    for (byte i = 0; i < numberOfSteps; i++) {
      time_hue[i] = (byte)doc["ratio"][i];
    }
  }
  if(doc.containsKey("ntpServer")){
    strcpy(time_server, doc["ntpServer"]);
    timeClient.setPoolServerName(time_server);
  }
  if(doc.containsKey("utcOffset")){
    time_utc_offset = (int)doc["utcOffset"];
    timeClient.setTimeOffset(time_utc_offset * 60);
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

  WiFiManagerParameter setting_hostname("hostname", "Devicename: (e.g. <code>smartlight-kitchen</code>)", hostname, 32);
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

    doc["hostname"] = setting_hostname.getValue();

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

void broadcastCurrentState(unsigned int messageID) {
  // webSocket.broadcastTXT("{\"color\":{\"1\":" + String(currentOutput.a) + ",\"2\":" + String(currentOutput.b) + "}}");
  String state;
  switch (currentState){
    case STATE_OFF:
      state = "OFF";
      break;
    case STATE_MANUAL:
      state = "MANUAL";
      break;
    case STATE_TIME:
      state = "TIME";
      break;
  }
  webSocket.broadcastTXT("{\"action\":\"GET /output\",\"id\":" + String(messageID) +
    ",\"data\":{\"channel\":[" +
      String(currentOutput.a) + "," + String(currentOutput.b) +
    "],\"brightness\":" + String(brightness) +
    ",\"ratio\":" + String((byte)(hue * 100)) +
    ",\"power\":" + ((currentOutput.a == 0 && currentOutput.b == 0) ? "false" : "true") +
    ",\"time\":\"" + String(timeClient.getHours() % 24) + ":" + String(timeClient.getMinutes() % 60) +
    "\",\"state\": \"" + String(state) +
    "\"}}");
}

void sendOK(byte target, unsigned int messageID){
  webSocket.sendTXT(target, "{\"status\":\"OK\", \"id\":" + String(messageID)+ "}");
}

bool getStateByColor(Channels output){
  return output.a == 0 && output.b == 0 ? STATE_OFF : STATE_MANUAL;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_TEXT:{
        char* json = (char *) &payload[0];
        DynamicJsonDocument doc(maxPayloadSize);
        auto error = deserializeJson(doc, json);
        if (error) {
          webSocket.sendTXT(num, "{\"status\":\"Error\",\"data\":\"Failed to parse payload\"}");
          return;
        }
        /* LEGACY - Should be removed when Website is updated */
        if(doc.containsKey("color")){
          currentOutput = {
            doc["color"]["1"],
            doc["color"]["2"]
          };
          currentState = getStateByColor(currentOutput);
          broadcastCurrentState(0);
          return;
        }
        /* LEGACY END */
        if(!doc.containsKey("action")){
          webSocket.sendTXT(num, "{\"status\":\"Error\",\"data\":\"Payload has no action\"}");
          return;
        }
        String action = String((const char*)doc["action"]);

        unsigned int messageID = 0;
        if(doc.containsKey("id")){
          messageID = doc["id"];
        }


        if (action == "GET /output") {
          broadcastCurrentState(messageID);


        } else if (action == "SET /output/channel") {
          currentOutput = {
            (byte)doc["data"][0],
            (byte)doc["data"][1]
          };
          currentState = getStateByColor(currentOutput);
          sendOK(num, messageID);
          broadcastCurrentState(messageID);
        } else if (action == "GET /output/channel") {
          webSocket.sendTXT(num, "{\"action\":\"GET /output/channel\",\"data\":[" +
            String(currentOutput.a) + "," +
            String(currentOutput.b) + "]}");


        } else if (action == "SET /output/power") {
          if (doc["data"] == 0) {
            currentOutput = OFF;
            currentState = STATE_OFF;
          } else {
            currentState = STATE_TIME;
          }
          sendOK(num, messageID);
          broadcastCurrentState(messageID);
        } else if (action == "GET /output/power") {
          if (currentOutput.a == 0 && currentOutput.b == 0) {
            webSocket.sendTXT(num, "{\"action\":\"GET /output/power\",\"id\":" + String(messageID) + ",\"data\":0}");
          } else {
            webSocket.sendTXT(num, "{\"action\":\"GET /output/power\",\"id\":" + String(messageID) + ",\"data\":1}");
          }


        } else if (action == "SET /output/ratio") {
          hue = (byte)doc["data"] / 100.0;
          updateLED();
          currentState = getStateByColor(currentOutput);
          sendOK(num, messageID);
          broadcastCurrentState(messageID);
        } else if (action == "GET /output/ratio") {
          webSocket.sendTXT(num, "{\"action\":\"GET /output/ratio\",\"id\":" + String(messageID) + ",\"data\":" + String((byte)(hue * 100)) + "}");


        } else if (action == "SET /output/brightness") {
          brightness = (byte)doc["data"];
          updateLED();
          currentState = getStateByColor(currentOutput);
          sendOK(num, messageID);
          broadcastCurrentState(messageID);
        } else if (action == "GET /output/brightness") {
          webSocket.sendTXT(num, "{\"action\":\"GET /output/brightness\",\"id\":" + String(messageID) + ",\"data\":" + String(brightness) + "}");


        } else if (action == "SET /output/brightness-and-ratio") {
          brightness = (byte)doc["data"][0];
          hue = (byte)doc["data"][1] / 100.0;
          updateLED();
          currentState = getStateByColor(currentOutput);
          sendOK(num, messageID);
          broadcastCurrentState(messageID);
        } else if (action == "GET /output/brightness-and-ratio") {
          webSocket.sendTXT(num, "{\"action\":\"GET /output/brightness-and-ratio\",\"id\":" + String(messageID) + ",\"data\":["
            + String(brightness) + ","
            + String((byte)(hue * 100))
          + "]}");


        } else if (action == "SET /settings/daylight") {
          File configFile = filesystem->open(PATH_CONFIG_TIME, "w");
          serializeJson(doc["data"], configFile);
          configFile.close();
          setupTimeConfig();
          sendOK(num, messageID);
        } else if (action == "GET /settings/daylight") {
          String hueList = "";
          String brightnessList = "";
          for (byte i = 0; i < 24; i++) {
            hueList += String(time_hue[i]);
            brightnessList += String(time_brightness[i]);
            if(i < 23){
              hueList += ",";
              brightnessList += ",";
            }
          }
          webSocket.sendTXT(num, "{\"action\":\"GET /settings/daylight\",\"id\":" + String(messageID) + ",\"data\":{\"ratio\":["
            + String(hueList)
            + "],\"brightness\":["
            + String(brightnessList)
            + "],\"utcOffset\":"
            + String(time_utc_offset)
            + ",\"ntpServer\":\""
            + String(time_server)
            + "\"}}");


        } else if (action == "SET /settings/connection") {
          File configFile = filesystem->open(PATH_CONFIG_WIFI, "w");
          serializeJson(doc["data"], configFile);
          configFile.close();
          sendOK(num, messageID);
          ESP.restart();
        } else if (action == "GET /settings/connection") {
          webSocket.sendTXT(num, "{\"action\":\"GET /settings/connection\",\"id\":" + String(messageID) + ",\"data\":{\"hostname\":"
            + String(hostname)
            + "\"}}");


        } else {
          webSocket.sendTXT(num, "{\"status\":\"Error\",\"id\":" + String(messageID) + ",\"data\":\"Unknown Payload\"}");
          return;
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

#define TOUCHED true
#define RELEASED false

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

bool isBtn(bool state = TOUCHED, unsigned long debounceDuration = 75) {
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
    delay(min(debounceDuration, (unsigned long)5));
  } while(millis() - touchStart < debounceDuration);
  return (match >= noMatch * 2);
}

bool waitForBtn(int ms, bool state = TOUCHED) {
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
  if (waitForBtn(0, TOUCHED)) {
    if (waitForBtn(SHORT_PRESS_TIMEOUT, RELEASED)) {
      if (waitForBtn(SHORT_PRESS_TIMEOUT, TOUCHED)) {
        if (waitForBtn(SHORT_PRESS_TIMEOUT, RELEASED)) {
          if (waitForBtn(SHORT_PRESS_TIMEOUT, TOUCHED)) {
            if (waitForBtn(SHORT_PRESS_TIMEOUT, RELEASED)) {
              if (waitForBtn(SHORT_PRESS_TIMEOUT, TOUCHED) && !waitForBtn(SHORT_PRESS_TIMEOUT, RELEASED)) {
                // tap tap tap hold
                currentState = getStateByColor(currentOutput);
                // cycle hue +
                while (hue + HUE_STEP <= HUE_MAX && isBtn(TOUCHED, HUE_STEP_DURATION)) {
                  hue += HUE_STEP;
                  updateLED();
                }
                
              }
            } else {
              // tap tap hold
              currentState = getStateByColor(currentOutput);
              // cycle hue -
              while (hue - HUE_STEP >= HUE_MIN && isBtn(TOUCHED, HUE_STEP_DURATION)) {
                hue -= HUE_STEP;
                updateLED();
              }
            }
          } else {
            // tap tap
          }
        } else {
          // tap, hold
          currentState = STATE_MANUAL;
          // increase brightness
          while (brightness + BRIGHNESS_STEP <= BRIGHTNESS_MAX && isBtn(TOUCHED, BRIGHNESS_STEP_DURATION)) {
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
      currentState = STATE_MANUAL;
      // reduce brightness
      while (brightness - BRIGHNESS_STEP >= BRIGHTNESS_MIN && isBtn(TOUCHED, BRIGHNESS_STEP_DURATION)) {
        brightness -= BRIGHNESS_STEP;
        updateLED();
        currentState = getStateByColor(currentOutput);
      }
      currentState = getStateByColor(currentOutput);
    }
    waitForBtn(TIMEOUT_INFINITY, RELEASED);
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
  brightness = map(minutes, 0, 60, time_brightness[(hour) % 24], time_brightness[(hour + 1) % 24]);
  hue = map(
      minutes, 0, 60,
      (unsigned int)((time_hue[(hour) % 24] / 100.0) * STEP_PRECISION),
      (unsigned int)((time_hue[(hour + 1) % 24] / 100.0)* STEP_PRECISION)
    ) / STEP_PRECISION;
  if(millis() - lastTimeSend > 1000){
    broadcastCurrentState(0);
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

  blink(ON, 1, 300);
  setupWifi();
  blink(ON, 2, 300);
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
    case STATE_OFF:
    case STATE_MANUAL: {
        if(lastOutput.a != currentOutput.a || lastOutput.b != currentOutput.b){
          setOutput(currentOutput);
          lastOutput = currentOutput;
          brightness = getBrightness(currentOutput);
          hue = getHue(currentOutput);
        }
      }
      break;
    case STATE_TIME: {
        setByTime();
      }
      break;
  }
}
