#define configFilePath "config.json"
#define JSON_HOSTNAME "hostname"
#define JSON_LAMP_TYPE "lamptype"

WiFiManagerParameter setting_hostname(JSON_HOSTNAME, "Devicename: (e.g. <code>smartlight-kitchen</code>)", hostname, 32);
WiFiManagerParameter setting_lamptype(JSON_LAMP_TYPE, "Type of connected lamp:<br /><span>Options: <code>NeoPixel</code>, <code>Analog</code></span>", lamptype, 32);

void saveConfigCallback () {
  const size_t capacity = JSON_OBJECT_SIZE(2);
  DynamicJsonDocument doc(capacity);

  doc[JSON_HOSTNAME] = setting_hostname.getValue();
  doc[JSON_LAMP_TYPE] = setting_lamptype.getValue();

  File configFile = SPIFFS.open(configFilePath, "w");
  serializeJson(doc, configFile);
  configFile.close();

  setColor(GREEN);
  delay(1000);
  setColor(BLACK);
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
        
        const size_t capacity = JSON_OBJECT_SIZE(2) + 70;
        DynamicJsonDocument doc(capacity);

        auto error = deserializeJson(doc, buf.get());
        if(error){
          return;
        }
        // copy from config to variable
        if(doc.containsKey(JSON_HOSTNAME)){
          strcpy(hostname, doc[JSON_HOSTNAME]);
        }
        if(doc.containsKey(JSON_LAMP_TYPE)){
          strcpy(lamptype, doc[JSON_LAMP_TYPE]);
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

  setColor(VIOLET);

  if(!wm.autoConnect("SmartLight Setup", "LightItUp")){
    setColor(RED);
    // shut down till the next reboot
    //ESP.deepSleep(86400000000); // 1 Day
    ESP.deepSleep(600000000); // 10 Minutes
    ESP.restart();
  }
  setColor(WHITE);
}
