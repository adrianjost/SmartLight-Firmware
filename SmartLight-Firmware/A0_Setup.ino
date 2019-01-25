#define configFilePath "config.json"
#define JSON_HOSTNAME "hostname"
#define JSON_LAMP_TYPE "lamptype"
#define JSON_PINOUT "analogpinout"

char hostname[32];
char lamptype[32]; // "NeoPixel", "Analog"
char pinoutCharArray[4]; // RGB, BGR, ...

WiFiManagerParameter setting_hostname(JSON_HOSTNAME, "Devicename: (e.g. <code>smartlight-kitchen</code>)", hostname, 32);
WiFiManagerParameter setting_lamptype(JSON_LAMP_TYPE, "Type of connected lamp:<br /><span>Options: <code>NeoPixel</code>, <code>Analog</code></span>", lamptype, 32);
WiFiManagerParameter setting_pinout(JSON_PINOUT, "Pinout of Analog Wires:<br /><span>Options: <code>RGB</code>, <code>RBG</code>, ...</span>", pinoutCharArray, 4);

void saveConfigCallback () {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  json[JSON_HOSTNAME] = setting_hostname.getValue();
  json[JSON_LAMP_TYPE] = setting_lamptype.getValue();
  json[JSON_PINOUT] = setting_pinout.getValue();

  File configFile = SPIFFS.open(configFilePath, "w");
  json.printTo(configFile);
  configFile.close();

  setColor(RGB{0,55,0});
  delay(1000);
  ESP.restart();
}

void setupSpiffs(){
  // initial values
  ("SmartLight-" + String(ESP.getChipId(), HEX)).toCharArray(hostname, 32);

  if (SPIFFS.begin()) {
    if (SPIFFS.exists(configFilePath)) {
      //file exists, reading and loading
      File configFile = SPIFFS.open(configFilePath, "r");
      if (configFile) {
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        if (json.success()) {
          // copy from config to variable
          if(json.containsKey(JSON_HOSTNAME)){
            strcpy(hostname, json[JSON_HOSTNAME]);
          }
          if(json.containsKey(JSON_LAMP_TYPE)){
            strcpy(lamptype, json[JSON_LAMP_TYPE]);
          }
          if(json.containsKey(JSON_PINOUT)){
            strcpy(pinoutCharArray, json[JSON_PINOUT]);
          }
        }
      }
    }
  }
}

void setupWifi(){
  WiFiManager wm;
  wm.setDebugOutput(false);
  wm.setTimeout(300);
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.setHostname(hostname);

  wm.addParameter(&setting_hostname);
  wm.addParameter(&setting_lamptype);
  wm.addParameter(&setting_pinout);

  setColor(RGB{55,0,55});

  if(!wm.autoConnect("SmartLight Setup", "LightItUp")){
    setColor(RGB{255,0,0});
    // shut down till the next reboot
    //ESP.deepSleep(86400000000); // 1 Day
    ESP.deepSleep(600000000); // 10 Minutes
    ESP.restart();
  }
  setColor(RGB{55,55,55});
}