# SmartLight Firmware

[![Build](https://github.com/adrianjost/SmartLight-Firmware/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/adrianjost/SmartLight-Firmware/actions/workflows/build.yml)

## How to use

Please read the full installation guide:
https://docs.smartlight.ga/setup/build-the-hardware/install-firmware

## Recommended VSCode Settings

I recommend the Arduino VSCode Extension to trigger builds and uploads directly from VSCode.

### `.vscode/settings.json`

```js
{
  // Your Path to the Arduino directory
  "arduino.path": "C:/Programme (Portable)/arduino-1.8.10"
}
```

### `.vscode/arduino.json`

To get the board configuration, enable the extended output during compilation in the arduino IDE, then copy the output of the first line and select the part starting with `-fqbn=esp8266:esp8266:generic:`

```js
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

To identify the related answer to your request, you can send add a field `id` with an integer. This `id` is included in all answers related to your request. You can use this, to filter prevent feedback loops when watching for broadcasted state messages. If no `id` is given, `0` is used.

#### Get Current State

This action is a special case, because it does not have a set counterpart. To set a state, you need to use one of the more specific actions.

**Request:**
```js
{
  "action": "GET /output",
  "id": 12345
}
```
**Response:**
```js
{
  "action": "GET /output",
  "id": 12345,
  "data": {
    "channel": [0, 0],
    "brightness": 0,
    "ratio": 50,
    "power": false,
    "time": "6:34",
    "state": "OFF" // "MANUAL", "AUTO"
  }
}
```

#### Set Channels directly

Sets all channels to the defined values. The max value is `255`.

```js
{
  "action": "SET /output/channel",
  "id": 12345,
  "data": [/* value channel 1 */, /* value channel 2 */]
}
```

#### Set Channel Ratio

Sets the ratio between the left and the right channel as a number between `0` and `100`. `50` means, that both channels will use the full brightness, `25` means `channel 1` will be twice as bright as `channel 2`.

```js
{
  "action": "SET /output/ratio",
  "data": 100
}
```

#### Set Max Channel Brightness

Sets the brightness value of the brightest channel according to the current Channel Ratio in percentage. `255` means `100%`/`max` brightness, `0`is equal to `off`.

```js
{
  "action": "SET /output/brightness",
  "data": 255
}
```

#### Set brightness and ratio

Sets both the channel ratio and the max channel brightness with a single message.
Check `Set Channel Ratio` and `Set Brightness` for details.

```js
{
  "action": "SET /output/brightness-and-ratio",
  "data": [/* brightness */, /* ratio */]
}
```

#### Set Toggle Power State

Turns all channels on or off. If toggled on, the time based light settings are used

`0`: Off
`1`: On

```js
{
  "action": "SET /output/power",
  "data": 1
}
```

### Settings

#### Set daylight Settings

Sets the channel max brightness and channel ratio for each hour of the day. Starting with 00:00 UTC.
Also sets the UTC timezone offset in minutes.

```js
{
  "action": "SET /settings/daylight",
  "data": {
    "ratio": [], // 24 values between 0 and 100
    "brightness": [], // 24 values between 0 and 255
    "utcOffset": 60, // in minutes
    "ntpServer": "pool.ntp.org"
  }
}
```

#### Set Connection Settings

Updates the saved Connection configuration.

> ⚠️ **The Device will automatically reboot** after confirming the message to apply the changes. Your current connection will be closed and you need to reconnect.

```js
{
  "action": "SET /settings/connection",
  "data": {
    "hostname": "SmartLight-[CHIP-ID]"
  }
}
```

## OTA Updates

You can update the firmware using OTA-Updates (Over-The-Air Updates) so you don't need to connect the device physically to your PC. The [ArduinoOTA-Package](https://github.com/jandrassy/ArduinoOTA) is used for this, so please refer to [this documentation how to setup this update process](https://arduino-esp8266.readthedocs.io/en/latest/ota_updates/readme.html#arduino-ide). If you are prompted for the device password, please use the WiFi password from the WiFi the device is connected to.
