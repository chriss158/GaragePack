#include <ArduinoJson.h> 
#include <logger.h>


struct Attributes {
    bool open;
    String state;
    bool car;
    bool motion;
    int distance;
    int wifiQuality;
    unsigned long lastUpdate;
};

struct Sensordata{
    int distance;
    bool motion;
    unsigned long lastUpdate;
} ;

struct Settings {
    int doorDistanceOpen = 20;
    int doorDistanceClosed = 180;
    int doorTimeToggle = 20;
    unsigned long int relayMode = 500;
    bool hasPIR = false;
    bool hasCloseContact = false;
    const char* wifiSSID = "";
    const char* wifiPassword = "";
    const char* wifiHostname = "GaragePack";
    int restartAfterWifiError = 120;

    const char* mqttHost = "192.168.12.200";
    int mqttPort = 1883;
    const char* mqttLogin = "";
    const char* mqttPassword = "";
    bool runSetup = false;
    int errorCount = 0;
    unsigned long int doorTimeOpen = 10000;
    unsigned long int doorTimeClose = 10000;
};

// --------------------------------------------------------------------

String AttributesToJson(Attributes a) {
    String output;
    StaticJsonDocument<200> doc;
    doc["open"] = a.open;
    doc["state"] = a.state;
    doc["car"] = a.car;
    doc["motion"] = a.motion;
    doc["distance"] = a.distance;
    doc["wifiQuality"] = a.wifiQuality;
    doc["lastupdate"] = a.lastUpdate;
    serializeJson(doc, output);
    return output;
}

String sensordataToJson(Sensordata sd) {
    String output;
    StaticJsonDocument<200> doc;
    doc["distance"] = sd.distance;
    doc["motion"] = sd.motion;
    serializeJson(doc, output);
    return output;
}

String SettingsToJson(Settings sd) {
    String output;
    StaticJsonDocument<2000> doc;

    doc["doorDistanceOpen"] = sd.doorDistanceOpen;
    doc["doorDistanceClosed"] = sd.doorDistanceClosed;
    doc["doorTimeToggle"] = sd.doorTimeToggle;
    doc["hasPIR"] = sd.hasPIR;
    doc["hasCloseContact"] = sd.hasCloseContact;
    doc["relayMode"] = sd.relayMode;
    doc["wifiSSID"] = sd.wifiSSID;
    doc["wifiPassword"] = sd.wifiPassword;
    doc["wifiHostname"] = sd.wifiHostname;
    doc["mqttHost"] = sd.mqttHost;
    doc["mqttLogin"] = sd.mqttLogin;
    doc["mqttPassword"] = sd.mqttPassword;
    doc["runSetup"] = sd.runSetup;
    doc["restartAfterWifiError"] = sd.restartAfterWifiError;
    doc["errorCount"] = sd.errorCount;
    doc["doorTimeOpen"] = sd.doorTimeOpen;
    doc["doorTimeClose"] = sd.doorTimeClose;

    serializeJson(doc, output);
    return output;
}

Settings JsonToSettings(String json){
    Settings sd;
    StaticJsonDocument<2000> doc;
    DeserializationError error = deserializeJson(doc, json);

    // Test if parsing succeeds.
    if (error) {
        Logger.println("deserializeJson settings. RC:" + String(error.c_str()));
    } else {
        sd.doorDistanceOpen = doc["doorDistanceOpen"];
        sd.doorDistanceClosed = doc["doorDistanceClosed"];
        sd.doorTimeToggle = doc["doorTimeToggle"];
        sd.relayMode = doc["relayMode"];
        sd.hasPIR = doc["hasPIR"];
        sd.hasCloseContact = doc["hasCloseContact"];
        sd.wifiSSID = doc["wifiSSID"];
        sd.wifiPassword = doc["wifiPassword"];
        sd.wifiHostname = doc["wifiHostname"];
        sd.mqttHost = doc["mqttHost"];
        sd.mqttLogin = doc["mqttLogin"];
        sd.mqttPassword = doc["mqttPassword"];
        sd.runSetup = doc["runSetup"];
        sd.restartAfterWifiError = doc["restartAfterWifiError"];
        sd.doorTimeOpen = doc["doorTimeOpen"];
        sd.doorTimeClose = doc["doorTimeClose"];

        if(doc.containsKey("errorCount"))
        {
            sd.errorCount = doc["errorCount"];
        }
        else sd.errorCount = 0;
    }
    return sd;
}