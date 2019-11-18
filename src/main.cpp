#include <Arduino.h>

#include <Ultrasonic.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

#include <structs.h>
#include <garage.h>
#include <asyncwebserver.h>
#include <mqtt.h>

Settings settings;
// const char* ssid = "BND Servicemobil";
// const char* password = "bfb0zekczb";

// defines pins numbers
int relay = D5;
int pirPin = D7;
Ultrasonic ultrasonic1(D1, D2, 20000UL);

// unsigned long int relayMode = 500;  //0=toggle, >0=delay ms for push button emulation


const char* version = "0.15";

bool debug = false;
Sensordata sensors;
int prevChecksum = -33;
unsigned long nextScan=0;
unsigned long scanTime = 100;

int previousQuality = -1;
unsigned long wifiQualityCheckTime = 0;
unsigned long wifiQualityCheckTimer = 10000;

bool restartRequired = false;  
const uint setupAddr = 0;
bool setupMqttOk;

int errorCountTillSetup = 5;

// if ultrasonic sensor is <maxDistanceToConfig> for <configAry[seconds]>, start config portal
int maxDistanceToConfig = 5; 
unsigned int SecondsToConfig = 10;
unsigned long configStartTime = 0;

// failsafes and wifi checking values
unsigned int restartAfterWifiError = 60;
unsigned long timeToRestart = 0;
Garage garage;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
String currentTime = "";

// -------------------- internal functions ---------------------
bool saveSettings(Settings sd){
  File file = SPIFFS.open("/settings.json", "w");
  if(!file){
     Serial.println("There was an error opening the file for writing");
     return false;
  }
  if(file.print(SettingsToJson(sd))) {
    Serial.println("Settings written");
  }else {
      Serial.println("writing settings failed");
      return false;
  }
  return true;

}

int getDistance() { 
  unsigned int distance = ultrasonic1.read(CM);
   return distance;
  //return 200;
}

bool getMotion(){
  int val = digitalRead(pirPin);
  //low = no motion, high = motion
  if (val == LOW)
  {
    return false;
  }
  return true;
}

int getStatusChecksum(Status s){
  int ret = 0;
  if(s.open){
    ret+=1;
  }
  if(s.car){
    ret+=10;
  }
  if(s.motion){
    ret+=100;
  }

  if(s.state == "open")
  {
    ret+=1000;
  }
  else if(s.state == "closed") {
    ret+=1010;
  }
  else if(s.state == "opening") {
    ret+=1020;
  }
  else if(s.state == "closing") {
    ret+=1030;
  }
  return ret;
}
void resetErrorCounter() {
      settings.errorCount = 0;
      saveSettings(settings);
}
void increaseErrorCounter() {

  if(!settings.runSetup)
  {
      if(settings.errorCount < errorCountTillSetup)
      {
        settings.errorCount = settings.errorCount+1;
        String msg = "Setting error count till setup mode to ";
        msg+= String(settings.errorCount) + "/" + String(errorCountTillSetup);
        Serial.println(msg);
      }
      if(settings.errorCount >= errorCountTillSetup)
      {
        Serial.println("Error count reached. Run setup mode next restart.");
        settings.errorCount = 0;
        settings.runSetup = true;
      }
      saveSettings(settings);
  }
}
Wifi getQuality() {
  Wifi sd;
  if (WiFi.status() != WL_CONNECTED)
  {
    sd.wifiQuality = -1;
    return sd;
  }
  int dBm = WiFi.RSSI();
  if (dBm <= -100)
  {
    sd.wifiQuality = 0;
    return sd;
  }
  if (dBm >= -50)
  {
    sd.wifiQuality = 100;
    return sd;
  }
  sd.wifiQuality = 2 * (dBm + 100);
  return sd;
}
// ------------------------------ Setup -------------------------------------
void setup() {
  Serial.begin(115200); // Starts the serial communication
  
  Serial.print("Garagepack Version ");
  Serial.println(version);

  pinMode(relay, OUTPUT);



  //read config
  SPIFFS.begin();
  File f = SPIFFS.open("/settings.json", "r");
  if (!f) {
     Serial.println("Loading settings failed! Either the SPIFFS filesystem was not flashed or something went horribly wrong.");
  }else {
    String jdata = f.readString();
    settings = JsonToSettings(jdata);

    Serial.println("Loading settings");   
  } 

  garage.initGarage(settings, relay);

  // delay(1000);
  startWifi();
  Serial.println(WiFi.status());
  if(WiFi.status() != WL_CONNECTED){
    if(!settings.runSetup)
    {
      Serial.println("Wifi connection error! restarting in seconds " + String(restartAfterWifiError));
      timeToRestart = millis() + (restartAfterWifiError * 1000);
      increaseErrorCounter();
    }
  }
  else
  {
    resetErrorCounter();
  }
  

  timeClient.begin();
  if(String(settings.mqttHost)!="" && WiFi.status() == WL_CONNECTED){
    setupMqtt();
    setupMqttOk = true;
  } else {
    Serial.println("MQTT host not set, skipping");
    setupMqttOk = false;
  }
}





// ------------------ main loop


void loop() {
  
  // check wifi
  if ( !settings.runSetup and (WiFi.status() == WL_CONNECTION_LOST || WiFi.status() == WL_DISCONNECTED))
  {
    Serial.println("connection to WiFi lost, restarting");
    restartRequired = true;
  }

  // check for restart flag or restart timer
  if (restartRequired || (timeToRestart>0 and timeToRestart< millis() )){  // check the flag here to determine if a restart is required
    Serial.printf("Restarting ESP\n\r");
    restartRequired = false;
    ESP.restart();
  }

  if(setupMqttOk){
    mqttLoop();
  }

  // get time for mqtt and mserial messages;
  currentTime = timeClient.getFormattedTime();

  bool stable = false;
  garage.checkTimers(); // cant use delay because of asyncwebserver
  sensors.lastUpdate = millis();

  // if PIR enabled, initialize
  if(settings.hasPIR){
    sensors.motion = getMotion();
  }

  //dont scan for distance all the time (echos?)
  if(nextScan<=millis()){
    nextScan = millis() + scanTime;
    sensors.distance = getDistance();
    stable = garage.feed(sensors);
    if(debug){
      Serial.println(sensors.distance);
    }
  }

  // -----  start config? check here
  if(sensors.distance <= maxDistanceToConfig && sensors.distance>0){
    if(configStartTime==0){
      configStartTime = millis();
      Serial.println("Config mode initializing");
    } 
  } else if (configStartTime>0) {
      Serial.println("Config mode cancled");
      configStartTime = 0;
  }
  
  // if scandistance lower <maxDistanceToConfig> for <SecondsToConfig> seconds
  // start config mode
  if(configStartTime >0 && (millis()-configStartTime)/1000 >= SecondsToConfig){
    configStartTime = 0;
    Serial.println("Starting Config Portal");
    settings.runSetup = true; //save in settings, will be reset once settings have been saved via UI aggain
    saveSettings(settings);
    restartRequired = true;
  }
  // -------------------------------


  if(stable){ //only submit values if readings are stable
    String strStatus = StatusToJson(garage.getStatus());
    char charStatus[255];
    strStatus.toCharArray(charStatus, 255);
    int currentChecksum = getStatusChecksum(garage.getStatus());

    if(currentChecksum != prevChecksum){
      mqttPublish(charStatus);
      Serial.println(strStatus);
      WifiSendtatus(strStatus);
    }
    prevChecksum = currentChecksum;

  }

  if(millis() > wifiQualityCheckTime)
  {
    Wifi quality = getQuality();
    if (quality.wifiQuality != previousQuality) {  
      if (quality.wifiQuality != -1)
      {
        Serial.printf("WiFi Quality:\t%d\%\tRSSI:\t%d dBm\r\n", quality, WiFi.RSSI());
        String wifiStatus = WifiToJson(quality);
        WifiSendtatus(wifiStatus);

      }
      previousQuality = quality.wifiQuality;
    }
    wifiQualityCheckTime = millis() + wifiQualityCheckTimer;
  }
}
