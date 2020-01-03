#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <logger.h>


extern Settings settings;
extern Garage garage;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;
char mqttTopicState[50] = "/state";
char mqttTopicCommand[50] = "/cmnd";
char mqttTopicAttributes[50] = "/attributes";

void callback(char *topic, byte *payload, unsigned int length)
{
  char msgin[200] = "";
  ;
  for (unsigned int i = 0; i < length; i++)
  {
    msgin[i] = (char)payload[i];
  }

  String strMsg = String(msgin);
  String strTopic = String(topic);
  Logger.println("MQTT Command: " +strMsg);
  strMsg.toLowerCase();
  if (String(mqttTopicCommand) == strTopic)
  {

    if (strMsg == "open")
    {
      garage.open();
    }
    else if (strMsg == "close")
    {
      garage.close();
    }
    else if (strMsg == "stop")
    {
      garage.stop();
    }
    else
    {
      Logger.println("Invalid");
    }
  }
}

void reconnect()
{
  // Loop until we're reconnected
  // while (!client.connected())
  if (!client.connected())
  {
    Logger.println("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    bool connectionOk = false;
    if (strlen(settings.mqttLogin) > 0 && strlen(settings.mqttPassword) > 0)
    {
      connectionOk = client.connect(clientId.c_str(), settings.mqttLogin, settings.mqttPassword);
    }
    else
    {
      connectionOk = client.connect(clientId.c_str());
    }

    if (connectionOk)
    {
      Logger.println("connected");
      Logger.println("MQTT publish: " + String(mqttTopicState) + ", " + String(mqttTopicAttributes));
      Logger.println("MQTT commands: " + String(mqttTopicCommand));

      client.subscribe(mqttTopicCommand);
    }
    else
    {
      Logger.println("failed, rc=" + String(client.state()));
      Logger.println("Failed, trying aggain next time");
      // Wait 5 seconds before retrying
      // delay(5000);
    }
  }
}

void setupMqtt()
{
  sprintf(mqttTopicCommand, "home/%s/cmnd", settings.wifiHostname);
  sprintf(mqttTopicState, "home/%s/state", settings.wifiHostname);
  sprintf(mqttTopicAttributes, "home/%s/attributes", settings.wifiHostname);
  client.setServer(settings.mqttHost, settings.mqttPort);
  client.setCallback(callback);
}

void mqttPublish(char topic[50], String msg)
{
  if (client.connected())
  {
    Logger.println("MQTT Publish Topic: " + String(topic) + ", Msg: " + msg);

    char charMsg[255];
    msg.toCharArray(charMsg, 255);
    client.publish(topic, charMsg);
  }
}

void mqttPublishState(String msg)
{
  mqttPublish(mqttTopicState, msg);
}

void mqttPublishAttributes(String msg)
{
  mqttPublish(mqttTopicAttributes, msg);
}

void mqttLoop()
{

  if (!client.connected())
  {
    reconnect();
  }
  client.loop();
}