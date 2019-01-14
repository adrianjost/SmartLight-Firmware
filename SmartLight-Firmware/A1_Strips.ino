#include <Adafruit_NeoPixel.h>

// Which pin on the ESP8266 is connected to the NeoPixels?
#define NEO_PIN 2
#define NEO_BRIGHTNESS 100
#define NEO_PIXELS 1
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NEO_PIXELS, NEO_PIN, NEO_GRB + NEO_KHZ800);

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
}
void initStrip(){
  // set new color
  // "NeoPixel", "Analog RGB"
  if(lamptype =="Analog RGB"){
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
  analogWrite(0,map(color.r,0,255,0,1024));
  analogWrite(1,map(color.g,0,255,0,1024));
  analogWrite(2,map(color.b,0,255,0,1024));
}
void setColor(RGB color){
  // set new color
  // "NeoPixel", "Analog RGB"
  if(lamptype == "Analog RGB"){
    setColorAnalog(color);
  }else{
    setColorNeoPixel(color);
  }
}
