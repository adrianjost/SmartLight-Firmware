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
    // TODO: check for array index out of range!
    // TODO: global variables need to be defined and set.
    gradientState++;
    RGB startColor = currentGradientColors[gradientState - 1];
    RGB targetColor = currentGradientColors[gradientState];
    unsigned long stopTimeStart = currentGradientTimes[gradientState - 1];
    unsigned long stopTimeTarget = currentGradientTimes[gradientState];
    unsigned long duration = stopTimeTarget - stopTimeStart;
    gradientInitFade(duration, startColor, targetColor);
    return;
  }
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