#include <Adafruit_NeoPixel.h>

// Which pin on the ESP8266 is connected to the NeoPixels?
#define NEO_PIN 2
#define NEO_BRIGHTNESS 100
#define NEO_PIXELS 300
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NEO_PIXELS, NEO_PIN, NEO_GRB + NEO_KHZ800);

RGB analogPinout = {0,1,2};

/**********************************
 INIT
**********************************/

void initStripNeoPixel(){
  pixels.begin(); // This initializes the NeoPixel library.
  pixels.setBrightness(NEO_BRIGHTNESS);
  pixels.show();
}
void initStripAnalog(){
  pinMode(0,OUTPUT);
  pinMode(1,OUTPUT);
  pinMode(2,OUTPUT);

       if(String(pinoutCharArray) == String("RBG")){ analogPinout = {0,2,1}; }
  else if(String(pinoutCharArray) == String("GBR")){ analogPinout = {1,2,0}; }
  else if(String(pinoutCharArray) == String("GRB")){ analogPinout = {1,0,2}; }
  else if(String(pinoutCharArray) == String("BRG")){ analogPinout = {2,0,1}; }
  else if(String(pinoutCharArray) == String("BGR")){ analogPinout = {2,1,0}; }
  else{ analogPinout = {0,1,2}; }
}
void initStrip(){
  // set new color
  // "NeoPixel", "Analog RGB"
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
  // "NeoPixel", "Analog RGB"
  if(String(lamptype) == String("Analog")){
    setColorAnalog(color);
  }else{
    setColorNeoPixel(color);
  }
}
