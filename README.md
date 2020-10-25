# SmartLight Firmware

[![Build Status](https://travis-ci.com/adrianjost/SmartLight-Firmware.svg?branch=master)](https://travis-ci.com/adrianjost/SmartLight-Firmware)

## How to use

Please read the full installation guide:
https://docs.smartlight.ga/setup/build-the-hardware/install-firmware

## Recommended VSCode Settings

I recommend the Arduino VSCode Extension to trigger builds and uploads directly from VSCode.

### `.vscode/settings.json`

```json
{
  // Your Path to the Arduino directory
  "arduino.path": "C:/Programme (Portable)/arduino-1.8.10"
}
```

### `.vscode/arduino.json`

To get the board configuration, enable the extended output during compilation in the arduino IDE, then copy the output of the first line and select the part starting with `-fqbn=esp8266:esp8266:generic:`

```json
{
    "sketch": "SmartLight-Firmware\\SmartLight-Firmware.ino",
    "board": "esp8266:esp8266:generic",
    "configuration": "xtal=80,vt=flash,exception=legacy,ssl=all,ResetMethod=nodemcu,CrystalFreq=26,FlashFreq=40,FlashMode=dout,eesz=1M128,led=1,sdk=nonosdk_191024,ip=lm2f,dbg=Disabled,lvl=None____,wipe=none,baud=115200"
}
```

## API

All Communication is done via WebSockets on Port 80

### Receives

#### Set color

Sets all channels to the defined values.

```json
{
  "type": "state",
  "data": {
    "type": "color",
    "data": [/* value channel 1 */, /* value channel 2 */, /* value channel 3 */]
  }
}
```

Each channel value must be an integer between 0 (inclusive) and 255 (inclusive).

#### Set gradient

Switches to gradient mode and fades between given colors according to the given configuration.

```json
{
  "type": "state",
  "data": {
    "type": "gradient",
    "data": {
      "colors": [
        // List of Colors, check the "Set color" section. The interface is identical
        // ...
      ],
      "transitionTimes": [/* duration of color n in ms as integer */], // check comment 1
      "loop": true // check comment 2
    }
  }
}
```

Comment 1: a colorTimestamp n declares when the nth color should be fully visible in milliseconds (1s = 1000ms).
In the given example, we start with a red color at 0s and transition smoothly to blue.
After 10s we are showing a solid blue color and begin fading to the 3th color (red).
After 10 more seconds (20s in total) we are showing full red again.
! the first value must be 0 and the total number of transitionTimes must be equal to the number of gradient colors. !

Comment 2: the loop option declares whether the gradient should start again when reaching the last color.
We do not do any type of fading here!
To make this transition smooth your gradient should start and end with the same colors.

#### turn on (TODO: find a better name)

Enabled "time mode" which selects the current channel value based on a saved list of colors for each hour of the day. Values between hours are interpolated on a linear basis.

```json
{
  "type": "state",
  "data": {
    "type": "time"
  }
}
```

#### configure time mode

Enabled "time mode" which selects the current channel value based on a saved list of colors for each hour of the day. Values between hours are interpolated on a linear basis.

```json
{
  "type": "config",
  "data": {
    "type": "time",
    "data": {
      
    }
  }
}
```

### Transmits

#### Current Color

Broadcasts the current color on every color change to all connected devices.
The send message uses the same interface as described in "Set color".

#### ~Current Gradient~ (not yet implemented)

Broadcasts the current gradient whenever a new gradient is set to all connected devices.
The send message uses the same interface as described in "Set gradient".
