#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define DEBUG_OUTPUT 0
#define MQTT_COMMAND_TOPIC "irpis/esp8266/command"
#define MQTT_SENDER "IRPIS-RPI"
#define MQTT_ACTION_ON "ON"
#define MQTT_ACTION_OFF "OFF"

// Set the WIFI SSID and password
// Replace with your SSID and password
const char* wifiSSID = "<your wifi ssid>";
const char* wifiPassword = "<your wifi password>";

// Set the MQTT server details and credential
const char* mqttServer = "<mqtt borker server ip>";
const char* clientID = "<desired client id>";
const char* mqttUser = "<mqtt user>";
const char* mqttPassword = "<mqtt password>";

WiFiClient espClient;
PubSubClient client(espClient);

/*
 * Initiate and connect to WiFi
 */
void initWiFi() {
  // Set WiFi mode
  WiFi.mode(WIFI_STA);

  // Enable reconnect in case of disconnection
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  // Connect to WiFi
  WiFi.begin(wifiSSID, wifiPassword);
  Serial.print("WiFi connecting ");
  Serial.println(wifiSSID);

  // Wait for the connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected to ");
  Serial.println(wifiSSID);
  Serial.print("IP address is ");
  Serial.println(WiFi.localIP());
  Serial.println("");
}

/*
 * Initiate and connect to MQTT server
 */
void initMqtt() {
  client.setServer(mqttServer, 1883);
  client.setCallback(callbackMqtt);
  connectMqtt();
}

/*
 * Connect to MQTT server
 */
void connectMqtt() {
  while (!client.connected()) {
    if (client.connect(clientID, mqttUser, mqttPassword)) {
      Serial.println("Connected to MQTT");
      client.subscribe(MQTT_COMMAND_TOPIC);
      Serial.print("Subscribed to topic ");
      Serial.println(MQTT_COMMAND_TOPIC);
    } else {
      Serial.print("Connection to MQTT failed. Return code is ");
      Serial.println(client.state());
      Serial.println("Retrying in 30 seconds");
      delay(30 * 1000);
    }
  }
}

/*
 * Callback function upon receiving message from MQTT
 */
void callbackMqtt(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.println("Message:");
  payload[length] = '\0';
  Serial.println((char*)payload);

  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload, length);
  String sender = doc["sender"];
  String action = doc["action"];
  long duration = doc["duration"];

  if (!((String)MQTT_SENDER).equals(sender)) {
    sendResponseMqtt(false, "Unknown sender " + sender);
  } else {
    if (action == (String)MQTT_ACTION_ON) {
      actionOn();
      sendResponseMqtt(true, "OK");
    } else if (action == (String)MQTT_ACTION_OFF) {
      actionOff();
      Serial.println("off");
      sendResponseMqtt(true, "OK");
    } else {
      sendResponseMqtt(false, "Unknown action " + action);
    }
  }
}

void actionOn() {
  Serial.println("on");
}

void actionOff() {
  Serial.println("on");
}

void sendResponseMqtt(bool status, String message) {
  Serial.print("Sending message ");
  Serial.println(message);
}

void setup() {
  // Set the BAUD rate to 115200
  Serial.begin(115200);
  // Set the debug output mode
  Serial.setDebugOutput(DEBUG_OUTPUT);

  // Setup the WiFi parameters and connect to it
  initWiFi();
  initMqtt();
}

void loop() {
  if (!client.connected()) {
    connectMqtt();
  }

  client.loop();
}
