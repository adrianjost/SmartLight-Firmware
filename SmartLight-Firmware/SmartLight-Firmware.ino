/**
Author: Adrian Jost
Version: 0.2.1
Date: 16 December 2018
**/
#include <ArduinoJson.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <Hash.h>

// WIFI-Manager
#include <FS.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#define STATE_UNDEFINED 0
#define STATE_COLOR 1
#define STATE_GRADIENT 2

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

//*************************
// global State
//*************************
byte currentState = STATE_UNDEFINED;
bool hasNewValue = false;

// currentState == STATE_COLOR
RGB currentColor {0,0,0};

// currentState == STATE_GRADIENT
RGB *currentGradientColors; // create pointer where informations can be stored
unsigned long *currentGradientTimes; // create pointer where informations can be stored
byte numberOfSteps;
bool currentGradientLoop;



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
          text = (text.substring(text.indexOf("J")+1, text.length()));
        }
        DynamicJsonBuffer jsonBuffer;
        JsonObject &tmpStateJson = jsonBuffer.parseObject(text);
        if (tmpStateJson.success()) {
          if(tmpStateJson.containsKey("color")){
            currentColor = {
              tmpStateJson["color"]["r"],
              tmpStateJson["color"]["g"],
              tmpStateJson["color"]["b"]
            };
            currentState = STATE_COLOR;
            hasNewValue = true;
          }else if(tmpStateJson.containsKey("gradient")){
            // TODO, copy gradient settings into global vars (& adjust dynamic array size)
            free(currentGradientColors);
            free(currentGradientTimes);
            numberOfSteps = tmpStateJson["gradient"]["colors"].size();
            currentGradientColors = (RGB *) malloc(sizeof(RGB) * numberOfSteps);
            currentGradientTimes = (unsigned long *) malloc(sizeof(unsigned long) * numberOfSteps);

            currentGradientLoop = tmpStateJson["gradient"]["loop"];
            for (int i = 0; i < numberOfSteps; i++) {
              (currentGradientColors)[i] = RGB {
                tmpStateJson["gradient"]["colors"][i]["r"],
                tmpStateJson["gradient"]["colors"][i]["g"],
                tmpStateJson["gradient"]["colors"][i]["b"]
              };
              (currentGradientTimes)[i] = tmpStateJson["gradient"]["transitionTimes"][i];
            }

            currentState = STATE_GRADIENT;
            hasNewValue = true;
          }else{
            currentState = STATE_UNDEFINED;
          }
        }else{
          // ERROR parsing State
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
  setupSpiffs();

  initStrip();

  setColor(RGB{0,0,55});
  setupWifi();
  setColor(RGB{0,0,0});

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
    currentState = STATE_UNDEFINED;
  }

  if(currentState == STATE_GRADIENT){
    if(!gradientLoop(hasNewValue)){
      setColor(RGB{r:255,g:255,b:255});
      delay(1000);
      currentState = STATE_UNDEFINED;
    }
  }
  hasNewValue = false;
}
