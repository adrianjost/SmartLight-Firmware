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

### Control

All `SET` Methods here have a `GET` counterpart which only requires the `action` attribute to be present. The Server will answer with the same action name and the additional data attribute.
If you send `{ "action": "GET /output/channel" }`, the server will answer with a message `{ "action": "GET /output/channel", "data": [/* value channel 1 */, /* value channel 2 */] }`

Each string value is only allowed to include max. 50 characters. If you use more, it's possible that the payload can not be parsed. The same issue can arise if you send additional properties.

#### Set Channels directly

Sets all channels to the defined values.

```json
{
  "action": "SET /output/channel",
  "data": [/* value channel 1 */, /* value channel 2 */]
}
```

#### Set Channel Ratio (TODO)

Sets the ratio between the left and the right channel as a number between `0` and `100`. `50` means, that both channels will use the full brightness, `25` means `channel 1` will be twice as bright as `channel 2`.

```json
{
  "action": "SET /output/ratio",
  "data": 100
}
```

#### Set Max Channel Brightness (TODO)

Sets the brightness value of the brightest channel according to the current Channel Ratio in percentage. `100` means `100%`/`max` brightness.

```json
{
  "action": "SET /output/brightness",
  "data": 100
}
```

### Set brightness and ratio (TODO)

Sets both the channel ratio and the max channel brightness with a single message.
Check `Set Channel Ratio` and `Set Brightness` for details.

```json
{
  "action": "SET /output/brightness-and-ratio",
  "data": [/* brightness */, /* ratio */]
}
```

### Set Toggle Power State (TODO)

Turns all channels on or off. If toggled on, the time based light settings are used

`0`: Off
`1`: On

```json
{
  "action": "SET /output/power",
  "data": 1
}
```

### Settings

### Set daylight Settings (TODO)

Sets the channel max brightness and channel ratio for each hour of the day. Starting with 00:00 UTC.
Also sets the UTC timezone offset in minutes.

```json
{
  "action": "SET /settings/daylight",
  "data": {
    "ratio": [], // 24 values between 0 and 100
    "brightness": [], // 24 values between 0 and 100
    "utcOffset": 60,
    "ntpServer": "pool.ntp.org"
  }
}
```

### Set Connection Settings (TODO)

Updates the saved Connection configuration.

```json
{
  "action": "SET /settings/connection",
  "data": {
    "hostname": "SmartLight-[CHIP-ID]"
  }
}
```
