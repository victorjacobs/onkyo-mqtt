#include <ESP8266WiFi.h>
#include <OnkyoRI.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>

// Wifi
#define WIFI_SSID "<wifi-name>"
#define WIFI_PASSWORD "<wifi-password>"

// MQTT
#define MQTT_SERVER "<mqtt-server-ip>"
#define MQTT_USER "<mqtt-username>"
#define MQTT_PASSWORD "<mqtt-password>"
#define MQTT_PORT 1883
#define AMP_COMMANDS_TOPIC "home/livingroom/amp/commands"
#define AMP_LOGS_TOPIC "home/livingroom/amp/logs"
#define AMP_STATE_TOPIC "home/livingroom/amp/state"
#define AMP_SOURCE_TOPIC "home/livingroom/amp/source"

// RI
#define ONKYO_PIN 5

// OTA
#define OTA_NAME "<ota-name>"
#define OTA_PASSWORD "<ota-password>"

WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);
OnkyoRI onkyoClient(ONKYO_PIN);

void setup() {
  Serial.begin(115200);

  setupWifi();
  setupOTA();
  connectPubSub();

  pubSubClient.publish(AMP_LOGS_TOPIC, "Started", true);
}

void setupOTA() {
  ArduinoOTA.setHostname(OTA_NAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.begin();
}

void setupWifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void connectPubSub() {
  pubSubClient.setServer(MQTT_SERVER, MQTT_PORT);
  pubSubClient.setCallback(callback);

  // Loop until we're reconnected
  while (!pubSubClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (pubSubClient.connect(MQTT_USER, MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected");
      pubSubClient.subscribe(AMP_COMMANDS_TOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.print(pubSubClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {
  if (!pubSubClient.connected()) {
    ESP.reset();
  }

  ArduinoOTA.handle();

  pubSubClient.loop();

  delay(500);
}

void callback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<300> payloadDocument;
  StaticJsonDocument<300> resultDocument;
  DeserializationError err = deserializeJson(payloadDocument, payload);

  if (err) {
    String payloadString = String((char *) payload);
    pubSubClient.publish(AMP_LOGS_TOPIC, (String("Failed to parse JSON: ") + payloadString).c_str(), true);
    return;
  }

  String sendTopic;
  bool failedToParse = false;

  if (payloadDocument.containsKey("state")) {
    String state = payloadDocument["state"];
    sendTopic = AMP_STATE_TOPIC;

    if (state == "on") {
      onkyoClient.send(0x2f);
      resultDocument["state"] = "on";
    } else if (state == "off") {
      onkyoClient.send(0xda);
      resultDocument["state"] = "off";
    } else if (state == "reset") {
      ESP.reset();
    } else {
      failedToParse = true;
    }
  } else if (payloadDocument.containsKey("source")) {
    String source = payloadDocument["source"];
    sendTopic = AMP_SOURCE_TOPIC;

    if (source == "cd") {
      onkyoClient.send(0x20);
      resultDocument["source"] = "cd";
    } else if (source == "dock") {
      onkyoClient.send(0x170);
      resultDocument["source"] = "dock";
    } else {
      failedToParse = true;
    }
  } else if (payloadDocument.containsKey("volume")) {
    String volume = payloadDocument["volume"];

    if (volume == "up") {
      onkyoClient.send(0x2);
      delay(250);
      onkyoClient.send(0x2);
    } else if (volume == "down") {
      onkyoClient.send(0x3);
      delay(250);
      onkyoClient.send(0x3);
    } else {
      failedToParse = true;
    }
  } else {
    failedToParse = true;
  }

  if (failedToParse) {
    pubSubClient.publish(AMP_LOGS_TOPIC, String("Failed to do something with JSON: ").c_str(), true);
    return;
  }

  char outputBuffer[300];
  serializeJson(resultDocument, outputBuffer);
  pubSubClient.publish(sendTopic.c_str(), outputBuffer, true);
}
