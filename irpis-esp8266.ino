#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define DEBUG_OUTPUT 0
#define MQTT_COMMAND_TOPIC "irpis/esp8266/command"
#define MQTT_RESPONSE_TOPIC "irpis/esp8266/response"
#define MQTT_SENDER "IRPIS-RPI"
#define MQTT_ACTION_ON "ON"
#define MQTT_ACTION_OFF "OFF"
#define SUCCESS_TRUE "true"
#define SUCCESS_FALSE "false"
#define NOTIFICATION_TYPE_WIFI "WIFI"
#define NOTIFICATION_TYPE_MQTT "MQTT"
#define PAYLOAD_GPIO_PIN 2
#define NOTIFICATION_LED_PIN 16
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
unsigned long aliveMillis = 0;
unsigned long activeStartMillis = 0;
unsigned long activeDurationMillis = 0;

// Prototype function with default argument values
void publishResponse(String type, bool success = true, String message = "");

/*
   Initiate and connect to WiFi
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
    Serial.print(".");
    showNotificationBeeps(NOTIFICATION_LED_PIN, NOTIFICATION_TYPE_WIFI);
  }
  Serial.println("");
  Serial.print("WiFi connected to ");
  Serial.println(wifiSSID);
  Serial.print("IP address is ");
  Serial.println(WiFi.localIP());
  Serial.println("");
}

/*
   Initiate and connect to MQTT server
*/
void initMqtt() {
  client.setServer(mqttServer, 1883);
  client.setCallback(callbackMqtt);
  connectMqtt();
}

/*
   Initiate the output notification LED pin
*/
void initNotificationLedPin(uint8_t pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
}

/*
   Initiate the output payload GPIO pin
*/
void initOutputPayloadPin(uint8_t pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
}

/*
   Shows the notification beeps based on the type
*/
void showNotificationBeeps(uint8_t pin, String notificationType) {
  if (notificationType == NOTIFICATION_TYPE_WIFI) {
    digitalWrite(pin, !digitalRead(pin));
    delay(100);
    digitalWrite(pin, !digitalRead(pin));
    delay(100);
    digitalWrite(pin, !digitalRead(pin));
    delay(100);
    digitalWrite(pin, !digitalRead(pin));
    delay(700);
  } else if (notificationType == NOTIFICATION_TYPE_MQTT) {
    digitalWrite(pin, !digitalRead(pin));
    delay(100);
    digitalWrite(pin, !digitalRead(pin));
    delay(100);
    digitalWrite(pin, !digitalRead(pin));
    delay(100);
    digitalWrite(pin, !digitalRead(pin));
    delay(100);
    digitalWrite(pin, !digitalRead(pin));
    delay(100);
    digitalWrite(pin, !digitalRead(pin));
    delay(500);
  } else {
    digitalWrite(pin, !digitalRead(pin));
    delay(300);
    digitalWrite(pin, !digitalRead(pin));
    delay(700);
  }
}

/*
   Connect to MQTT server
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

      for (unsigned int i = 0; i < 30; i++) {
        Serial.print(".");
        showNotificationBeeps(NOTIFICATION_LED_PIN, NOTIFICATION_TYPE_MQTT);
      }
      Serial.println("");
    }
  }
}

/*
   Callback function upon receiving message from MQTT
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
  unsigned long duration = doc["duration"];
  activeDurationMillis = duration * 1000;

  if (!((String)MQTT_SENDER).equals(sender)) {
    publishResponse(RESPONSE_TYPE_COMMAND, false, "Unknown sender " + sender);
  } else {
    if (action == (String)MQTT_ACTION_ON) {
      activatePayload(PAYLOAD_GPIO_PIN);
    } else if (action == (String)MQTT_ACTION_OFF) {
      deactivatePayload(PAYLOAD_GPIO_PIN);
    } else {
      publishResponse(RESPONSE_TYPE_COMMAND, false, "Unknown action " + action);
    }
  }
}

/*
   Turn on the output GPIO pin
*/
void activatePayload(uint8_t pin) {
  if (!isOutputOn(pin)) {
    Serial.println("Turning payload ON");
    digitalWrite(pin, LOW);
    activeStartMillis = millis();
    publishResponse(RESPONSE_TYPE_COMMAND);
  } else {
    String message = "Payload is already ON";
    Serial.println(message);
    publishResponse(RESPONSE_TYPE_COMMAND, false, message);
  }
}

/*
   Turn off the output GPIO pin
*/
void deactivatePayload(uint8_t pin) {
  if (isOutputOn(pin)) {
    Serial.println("Turning payload OFF");
    digitalWrite(pin, HIGH);
    activeDurationMillis = 0;
    publishResponse(RESPONSE_TYPE_COMMAND);
  } else {
    String message = "Payload is already OFF";
    Serial.println(message);
    publishResponse(RESPONSE_TYPE_COMMAND, false, message);
  }
}

/*
   Check wheter the output GPIO pin is on or off
*/
bool isOutputOn(uint8_t pin) {
  return !digitalRead(pin);  // @TODO remove '!' later
}

/*
   Publishes the response. The response could be for COMMAND or ALIVE. The response will vary based on success or failure
*/
void publishResponse(String type, bool success, String message) {
  String status = isOutputOn(PAYLOAD_GPIO_PIN) ? MQTT_ACTION_ON : MQTT_ACTION_OFF;
  String successStr = success ? SUCCESS_TRUE : SUCCESS_FALSE;
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
   The code that runs only once
*/
void setup() {
  // Set the BAUD rate to 115200
  Serial.begin(115200);
  // Set the debug output mode
  Serial.setDebugOutput(DEBUG_OUTPUT);

  // Initiate the notification LED and output GPIO pin
  initNotificationLedPin(NOTIFICATION_LED_PIN);
  initOutputPayloadPin(PAYLOAD_GPIO_PIN);

  // Setup the WiFi parameters and connect to it
  initWiFi();
  initMqtt();
}

/*
    The code that keeps running
*/
void loop() {
  unsigned long currentMillis = millis();

  if (!client.connected()) {
    connectMqtt();
  }

  client.loop();

  if ((currentMillis >= aliveMillis) && (unsigned long)(currentMillis - aliveMillis) >= ALIVE_PUBLISH_INTERVAL_MILLISEC) {
    aliveMillis = currentMillis;
    publishResponse(RESPONSE_TYPE_ALIVE);
  }

  if (isOutputOn(PAYLOAD_GPIO_PIN) && (currentMillis >= activeStartMillis) && (unsigned long)(currentMillis - activeStartMillis) > activeDurationMillis) {
    deactivatePayload(PAYLOAD_GPIO_PIN);
  }

  delay(50);
}
