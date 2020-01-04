#include <Arduino.h>

#include <Ultrasonic.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

#include <structs.h>
#include <garage.h>
#include <asyncwebserver.h>
#include <mqtt.h>
#include <EEPROM.h>
#include <logger.h>

#define EEPROM_SIZE 1

Settings settings;
// const char* ssid = "BND Servicemobil";
// const char* password = "bfb0zekczb";

// defines pins numbers
int relay = D5;
int reedPin = D6;
int pirPin = D7;
Ultrasonic ultrasonic1(D1, D2, 20000UL);

// unsigned long int relayMode = 500;  //0=toggle, >0=delay ms for push button emulation

const char *version = "0.16.2";

bool debug = false;
Sensordata sensors;
int prevChecksum = -33;
bool prevOpen;
unsigned long nextScan = 0;
unsigned long scanTime = 100;

int previousQuality = -1;
unsigned long wifiQualityCheckTime = 0;
unsigned long wifiQualityCheckTimer = 10000;

bool restartRequired = false;
bool espRestarting = false;
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
AsyncWebServer server(80);
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
String currentTime = "";

// -------------------- internal functions ---------------------
bool saveSettings(Settings sd)
{
  File file = SPIFFS.open("/settings.json", "w");
  if (!file)
  {
    Logger.println("There was an error opening the file for writing");
    return false;
  }
  if (file.print(SettingsToJson(sd)))
  {
    Logger.println("Settings written");
    file.close();
  }
  else
  {
    Logger.println("writing settings failed");
    file.close();
    return false;
  }
  return true;
}

int getDistance()
{
  unsigned int distance = ultrasonic1.read(CM);
  return distance;
  //return 200;
}

bool getMotion()
{
  int val = digitalRead(pirPin);
  //low = no motion, high = motion
  if (val == LOW)
  {
    return false;
  }
  return true;
}



int getAttributesChecksum(Attributes a)
{
  int ret = 0;
  if (a.open)
  {
    ret += 1;
  }
  if (a.car)
  {
    ret += 10;
  }
  if (a.motion)
  {
    ret += 100;
  }

  if (a.state == "open")
  {
    ret += 1000;
  }
  else if (a.state == "closed")
  {
    ret += 1010;
  }
  else if (a.state == "opening")
  {
    ret += 1020;
  }
  else if (a.state == "closing")
  {
    ret += 1030;
  }
  return ret;
}
void resetErrorCounter()
{
  int currentErrorCount = EEPROM.read(0);
  if (currentErrorCount != 0)
  {
    EEPROM.write(0, 0);
    EEPROM.commit();
  }
}
void increaseErrorCounter()
{

  if (!settings.runSetup)
  {
    int currentErrorCount = EEPROM.read(0);
    if (currentErrorCount < errorCountTillSetup)
    {
      currentErrorCount = currentErrorCount + 1;
      String msg = "Setting error count till setup mode to ";
      msg += String(currentErrorCount) + "/" + String(errorCountTillSetup);
      Logger.println(msg);
    }
    if (currentErrorCount >= errorCountTillSetup)
    {
      Logger.println("Error count reached. Run setup mode next restart.");
      currentErrorCount = 0;
      settings.runSetup = true;
      saveSettings(settings);
    }
    EEPROM.write(0, currentErrorCount);
    EEPROM.commit();
  }
}

// ------------------------------ Setup -------------------------------------
void setup()
{
  Serial.begin(115200);
  SPIFFS.begin();

  Logger.begin(&server);

  Logger.println("Garagepack Version " + String(version));

  espRestarting = false;

  EEPROM.begin(EEPROM_SIZE);

  pinMode(relay, OUTPUT);
  pinMode(reedPin, INPUT_PULLUP);

  //read config
  File f = SPIFFS.open("/settings.json", "r");
  if (!f)
  {
    Logger.println("Loading settings failed! Either the SPIFFS filesystem was not flashed or something went horribly wrong.");
  }
  else
  {
    String jdata = f.readString();
    f.close();
    settings = JsonToSettings(jdata);
    Logger.println("Loading settings done");
    Logger.println(jdata);
  }

  garage.begin(settings, relay, reedPin);

  // delay(1000);
  startWifi();
  if (WiFi.status() != WL_CONNECTED)
  {
    if (!settings.runSetup)
    {
      Logger.println("Wifi connection error! restarting in seconds " + String(restartAfterWifiError));
      timeToRestart = millis() + (restartAfterWifiError * 1000);
      increaseErrorCounter();
    }
  }
  else
  {
    resetErrorCounter();
  }
  timeClient.begin();
  timeClient.forceUpdate();
  if (String(settings.mqttHost) == "")
  {
    Logger.println("MQTT host not set. Skipping MQTT setup");
    setupMqttOk = false;
  }
  else if (WiFi.status() != WL_CONNECTED)
  {
    Logger.println("Wifi not conncted. Skipping MQTT setup");
    setupMqttOk = false;
  }
  else if ((String(settings.mqttLogin) != "" || String(settings.mqttPassword) != "") && (String(settings.mqttLogin) == "" || String(settings.mqttPassword) == ""))
  {
    if (String(settings.mqttLogin) == "")
    {
      Logger.println("MQTT Login not set. Skipping MQTT setup");
    }
    else if (String(settings.mqttPassword) == "")
    {
      Logger.println("MQTT Password not set. Skipping MQTT setup");
    }
    setupMqttOk = false;
  }
  else
  {
    setupMqttOk = true;
  }

  if (setupMqttOk)
  {
    setupMqtt();
  }
}

// ------------------ main loop

void loop()
{
  if (espRestarting)
    return;
  timeClient.update();
  // check wifi
  if (!settings.runSetup and (WiFi.status() == WL_CONNECTION_LOST || WiFi.status() == WL_DISCONNECTED))
  {
    Logger.println("connection to WiFi lost, restarting");
    restartRequired = true;
  }

  // check for restart flag or restart timer
  if (restartRequired || (timeToRestart > 0 and timeToRestart < millis()))
  { // check the flag here to determine if a restart is required
    Logger.println("Restarting ESP");
    restartRequired = false;
    espRestarting = true;
    ESP.restart();
    return;
  }

  if (setupMqttOk)
  {
    mqttLoop();
  }

  // get time for mqtt and mserial messages;
  currentTime = timeClient.getFormattedTime();

  bool stable = false;
  garage.loop();
  sensors.lastUpdate = millis();

  // if PIR enabled, initialize
  if (settings.hasPIR)
  {
    sensors.motion = getMotion();
  }

  //dont scan for distance all the time (echos?)
  if (nextScan <= millis())
  {
    nextScan = millis() + scanTime;
    sensors.distance = getDistance();
    stable = garage.feed(sensors);
    if (debug)
    {
      Logger.println(sensors.distance);
    }
  }

  // -----  start config? check here
  if (sensors.distance <= maxDistanceToConfig && sensors.distance > 0)
  {
    if (configStartTime == 0)
    {
      configStartTime = millis();
      Logger.println("Config mode initializing");
    }
  }
  else if (configStartTime > 0)
  {
    Logger.println("Config mode cancled");
    configStartTime = 0;
  }

  // if scandistance lower <maxDistanceToConfig> for <SecondsToConfig> seconds
  // start config mode
  if (configStartTime > 0 && (millis() - configStartTime) / 1000 >= SecondsToConfig)
  {
    configStartTime = 0;
    Logger.println("Starting Config Portal");
    settings.runSetup = true; //save in settings, will be reset once settings have been saved via UI aggain
    saveSettings(settings);
    restartRequired = true;
  }
  // -------------------------------

  if (stable)
  { //only submit values if readings are stable

    Attributes currentAttributes = garage.getAttributes();
    int currentAttributesChecksum = getAttributesChecksum(currentAttributes);
    if (currentAttributesChecksum != prevChecksum)
    {
      String strAttributes = AttributesToJson(currentAttributes);
      mqttPublishAttributes(strAttributes);
      WifiSendtatus(strAttributes);
    }
    prevChecksum = currentAttributesChecksum;

    if (currentAttributes.open != prevOpen)
    {
      mqttPublishOpen(currentAttributes.open);
    }
    prevOpen = currentAttributes.open;
  }
}