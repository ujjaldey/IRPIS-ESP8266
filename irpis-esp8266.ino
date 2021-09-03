#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define DEBUG_OUTPUT 0
#define MQTT_COMMAND_TOPIC "irpis/esp8266/command"
#define MQTT_RESPONSE_TOPIC "irpis/esp8266/response"
#define MQTT_SENDER "IRPIS-RPI"
#define MQTT_ACTION_ON "ON"
#define MQTT_ACTION_OFF "OFF"
#define OUTPUT_PIN 2
#define ALIVE_PUBLISH_INTERVAL_MILLISEC 10000
#define RESPONSE_TYPE_ALIVE "ALIVE"
#define RESPONSE_TYPE_COMMAND "COMMAND"

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
unsigned long previousMillis = 0;

// Prototype function with default argument values
void publishResponse(String type, bool success = true, String message = "");

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
 * Initiate the output pin
 */
void initOutputPin(uint8_t pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
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
    publishResponse(RESPONSE_TYPE_COMMAND, false, "Unknown sender " + sender);
  } else {
    uint8_t pin = OUTPUT_PIN;

    if (action == (String)MQTT_ACTION_ON) {
      actionOn(pin);
      publishResponse(RESPONSE_TYPE_COMMAND);
    } else if (action == (String)MQTT_ACTION_OFF) {
      actionOff(pin);
      publishResponse(RESPONSE_TYPE_COMMAND);
    } else {
      publishResponse(RESPONSE_TYPE_COMMAND, false, "Unknown action " + action);
    }
  }
}

/*
 * Turn on the output pin
 */
void actionOn(uint8_t pin) {
  Serial.println("Turning pin ON");
  digitalWrite(pin, LOW);
  
}

/*
 * Turn off the output pin
 */
void actionOff(uint8_t pin) {
  Serial.println("Turning pin OFF");
  digitalWrite(pin, HIGH);
  publishResponse(RESPONSE_TYPE_COMMAND);
}

/*
 * Check wheter the output pin is on or off
 */
bool isOutputOn(uint8_t pin) {
  return digitalRead(pin);
}

/*
 * Publishes the response. The response could be for COMMAND or ALIVE. The response will vary based on success or failure
 */
void publishResponse(String type, bool success, String message) {
  String status = (!isOutputOn(OUTPUT_PIN)) ? "ON" : "OFF";
  String successStr = success ? "true" : "false";
  String responseJsonStr;
  
  if (success) {
    responseJsonStr = "{\"sender\": \"IRPIS-ESP8266\", \"success\": \"" + successStr + "\", \"type\": \"" + type + "\", \"status\": \"" + status + "\"}";
  } else {
    responseJsonStr = "{\"sender\": \"IRPIS-ESP8266\", \"success\": \"" + successStr + "\", \"type\": \"" + type + "\", \"message\": \"" + message + "\"}";
  }

  Serial.print("Response Json is ");
  Serial.println(responseJsonStr);

  byte responseLength = responseJsonStr.length() + 1;
  char response[responseLength];
  responseJsonStr.toCharArray(response, responseLength);
  client.publish(MQTT_RESPONSE_TOPIC, response);
}

/*
 * The code that runs only once
 */
void setup() {
  // Set the BAUD rate to 115200
  Serial.begin(115200);
  // Set the debug output mode
  Serial.setDebugOutput(DEBUG_OUTPUT);

  // Setup the WiFi parameters and connect to it
  initWiFi();
  initMqtt();
  initOutputPin(OUTPUT_PIN);
}

/*
 * The code that keeps running
 */
void loop() {
  unsigned long currentMillis = millis();

  if (!client.connected()) {
    connectMqtt();
  }

  client.loop();
  
  if ((unsigned long)(currentMillis - previousMillis) >= ALIVE_PUBLISH_INTERVAL_MILLISEC) {
    previousMillis = currentMillis;
    publishResponse(RESPONSE_TYPE_ALIVE);
    delay(50);
  }
}
