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
#define AMP_TOPIC "home/livingroom/amp"

// RI
#define ONKYO_PIN 5

// OTA
#define OTA_NAME "<ota-name>"

int cmd = 0;
bool pause = true;

WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);
OnkyoRI onkyoClient(ONKYO_PIN);

void setup() {
  Serial.begin(115200);

  setupWifi();
  setupOTA();
  connectPubSub();

  pubSubClient.publish("home/livingroom/amp/live", "Up", true);
}

void setupOTA() {
  ArduinoOTA.setHostname(OTA_NAME);
  
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
      pubSubClient.subscribe(AMP_TOPIC);
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

  if (!pause) {
    onkyoClient.send(cmd);

    pubSubClient.publish("home/livingroom/amp/live", String(cmd, HEX).c_str(), true);

    cmd++;
  }

  delay(500);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  Serial.print(message);
  Serial.print(" ");

  unsigned long cmd = strtoul(message, '\0', 16);
  Serial.println(cmd);

  if (String(message) == "p") {
    pause = !pause;
  } else {
    onkyoClient.send(cmd);
    pubSubClient.publish("home/livingroom/amp/live", message, true);
  }
}
