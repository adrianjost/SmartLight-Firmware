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
  "type": "setOutput",
  "data": [/* value channel 1 */, /* value channel 2 */, /* value channel 3 */]
}
```

Each channel value must be an integer between 0 (inclusive) and 255 (inclusive).

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
