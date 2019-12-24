# SmartLight Firmware

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
