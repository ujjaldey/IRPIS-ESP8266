#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define DEBUG_OUTPUT 0
#define MQTT_COMMAND_TOPIC "irpis/esp8266/command"
#define MQTT_RESPONSE_TOPIC "irpis/esp8266/response"
#define MQTT_SENDER "IRPIS-RPI"
#define ESP_BOARD "IRPIS-ESP8266"
#define MQTT_ACTION_ON "ON"
#define MQTT_ACTION_OFF "OFF"
#define MQTT_ACTION_STATUS "STATUS"
#define SUCCESS_TRUE "true"
#define SUCCESS_FALSE "false"
#define NOTIFICATION_TYPE_WIFI "WIFI"
#define NOTIFICATION_TYPE_MQTT "MQTT"
#define PAYLOAD_GPIO_PIN 0
#define NOTIFICATION_LED_PIN 2
#define ALIVE_PUBLISH_INTERVAL_MILLISEC 5000
#define BREATHING_NOTIFICATION_INTERVAL_MILLISEC 2500
#define BREATHING_DELAY_MILLISEC 100
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
unsigned long breathingCounter = 0;
unsigned int executionIdGlobal = 0;

// Prototype function with default argument values
void publishResponse(String type, bool success = true, String message = "", unsigned long duration = 0, unsigned int executionId = 0);

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
  // Set the MQTT server and callback function
  client.setServer(mqttServer, 1883);
  client.setCallback(callbackMqtt);
  // Connect to MQTT broker
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
   Shows the breathing beeps according to whether the system is either in standby or active
*/
void showBreathingBeeps(uint8_t pin) {
  if (breathingCounter == 0) {
    digitalWrite(pin, LOW);
  } else if (breathingCounter == 5) {
    digitalWrite(pin, HIGH);
  } else if (isOutputOn(PAYLOAD_GPIO_PIN) && breathingCounter >= BREATHING_NOTIFICATION_INTERVAL_MILLISEC / BREATHING_DELAY_MILLISEC) {
    breathingCounter = -1;  // so that in next loop it becomes 0 and satisfies the first condition
  } else if (!isOutputOn(PAYLOAD_GPIO_PIN) && breathingCounter >= ALIVE_PUBLISH_INTERVAL_MILLISEC / BREATHING_DELAY_MILLISEC) {
    breathingCounter = -1;  // so that in next loop it becomes 0 and satisfies the first condition
  }

  breathingCounter++;
}

/*
   Shows the notification beeps based on the type
*/
void showNotificationBeeps(uint8_t pin, String notificationType) {
  // For WiFi, the notification is beep-beep-pause (total 1 sec)
  // For MQTT, the notification is beep-beep-beep-pause (total 1 sec)
  // Else, the notification is beep-pause (total 1 sec)
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
  // Keep checking until MQTT is connected
  while (!client.connected()) {
    // Connect to MQTT broker using the client id and credential
    if (client.connect(clientID, mqttUser, mqttPassword)) {
      Serial.println("Connected to MQTT");
      // Subscribe to command topic
      client.subscribe(MQTT_COMMAND_TOPIC);
      Serial.print("Subscribed to topic ");
      Serial.println(MQTT_COMMAND_TOPIC);
    } else {
      Serial.print("Connection to MQTT failed. Return code is ");
      Serial.println(client.state());
      Serial.println("Retrying in 30 seconds");

      // Wait before retrying
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

  // Deserialize the payload string to json object and parse the parameters
  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload, length);
  String sender = doc["sender"];
  String action = doc["action"];
  unsigned long duration = doc["duration"];
  unsigned int executionId = doc["execution_id"];

  // Validate and call necessary actions
  if (!((String)MQTT_SENDER).equals(sender)) {
    publishResponse(RESPONSE_TYPE_COMMAND, false, "Unknown sender " + sender, 0, executionId);
  } else if (action == (String)MQTT_ACTION_ON && duration <= 0) {
    publishResponse(RESPONSE_TYPE_COMMAND, false, "Duration " + ((String)duration) + " should be greater than 0 when the action is " + MQTT_ACTION_ON, 0, executionId);
  } else {
    if (action == (String)MQTT_ACTION_ON) {
      activatePayload(PAYLOAD_GPIO_PIN, duration, executionId);
    } else if (action == (String)MQTT_ACTION_OFF) {
      deactivatePayload(PAYLOAD_GPIO_PIN, executionId);
    } else if (action == (String)MQTT_ACTION_STATUS) {
      publishResponse(MQTT_ACTION_STATUS, true, "ESP8266 is up and running", (int)(activeDurationMillis / 1000), executionIdGlobal);
    } else {
      publishResponse(RESPONSE_TYPE_COMMAND, false, "Unknown action " + action, 0, executionId);
    }
  }
}

/*
   Turn on the output GPIO pin
*/
void activatePayload(uint8_t pin, unsigned long duration, unsigned int executionId) {
  // If the payload is already not active then turn it on, else return error
  if (!isOutputOn(pin)) {
    Serial.println("Turning payload on");
    // Assig the execution id to the global variable
    executionIdGlobal = executionId;
    digitalWrite(pin, LOW);
    // Assign the duration when the action is ON
    activeDurationMillis = duration * 1000;
    // Initialize the activeStartMillis with current millis
    activeStartMillis = millis();
    publishResponse(RESPONSE_TYPE_COMMAND, true, "", duration, executionId);
  } else {
    String message = "Irrigation is already on";
    Serial.println(message);
    publishResponse(RESPONSE_TYPE_COMMAND, false, message, duration, executionId);
  }
}

/*
   Turn off the output GPIO pin
*/
void deactivatePayload(uint8_t pin, unsigned int executionId) {
  // If the payload is already active then turn it off, else return error
  if (isOutputOn(pin)) {
    Serial.println("Turning payload off");
    // Assig the execution id to the global variable
    executionIdGlobal = executionId;
    digitalWrite(pin, HIGH);
    activeDurationMillis = 0;
    publishResponse(RESPONSE_TYPE_COMMAND, true, "", 0, executionId);
  } else {
    String message = "Irrigation is already off";
    Serial.println(message);
    publishResponse(RESPONSE_TYPE_COMMAND, false, message, 0, executionId);
  }

  // Reset the global variable after the payload is turned off
  executionIdGlobal = 0;
}

/*
   Check wheter the output GPIO pin is on or off
*/
bool isOutputOn(uint8_t pin) {
  // Read the pin and return
  return !digitalRead(pin);
}

/*
   Publishes the response. The response could be for COMMAND or ALIVE. The response will vary based on success or failure
*/
void publishResponse(String type, bool success, String message, unsigned long duration, unsigned int executionId) {
  String status = isOutputOn(PAYLOAD_GPIO_PIN) ? MQTT_ACTION_ON : MQTT_ACTION_OFF;
  String successStr = success ? SUCCESS_TRUE : SUCCESS_FALSE;
  String responseJsonStr;

  if (success) {
    responseJsonStr = "{\"sender\": \"" + String(ESP_BOARD) +
                      "\", \"success\": \"" + successStr +
                      "\", \"type\": \"" + type +
                      "\", \"status\": \"" + status +
                      "\", \"duration\": \"" + String(duration) +
                      "\", \"execution_id\": " + executionId +
                      ", \"message\": \"" + message + "\"}";
  } else {
    responseJsonStr = "{\"sender\": \"" + String(ESP_BOARD) +
                      "\", \"success\": \"" + successStr +
                      "\", \"type\": \"" + type +
                      "\", \"status\": \"" + status +
                      "\", \"duration\": \"" + String(duration) +
                      "\", \"execution_id\": " + executionId +
                      ", \"message\": \"" + message + "\"}";
  }

  Serial.print("Response Json is ");
  Serial.println(responseJsonStr);

  // Prepare the response payload
  byte responseLength = responseJsonStr.length() + 1;
  char response[responseLength];
  responseJsonStr.toCharArray(response, responseLength);
  // Publish the response to the response topic
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
  // Setup the millis() for the heartbeat
  unsigned long currentMillis = millis();

  // Keep checking whether MQTT is connected or not. Retry if not connected.
  if (!client.connected()) {
    connectMqtt();
  }

  // Keep the MQTT client active
  client.loop();

  // Auto turn off the payload once the activate period is over
  if (isOutputOn(PAYLOAD_GPIO_PIN) && (currentMillis >= activeStartMillis) && (unsigned long)(currentMillis - activeStartMillis) > activeDurationMillis) {
    deactivatePayload(PAYLOAD_GPIO_PIN, executionIdGlobal);
    // Reset the global variable after the payload is turned off
    executionIdGlobal = 0;
  }

  // Publish the response at intervals
  if ((currentMillis >= aliveMillis) && (unsigned long)(currentMillis - aliveMillis) >= ALIVE_PUBLISH_INTERVAL_MILLISEC) {
    aliveMillis = currentMillis;
    publishResponse(RESPONSE_TYPE_ALIVE, true, "", 0, executionIdGlobal);
  }

  // Show the breathing beeps
  showBreathingBeeps(NOTIFICATION_LED_PIN);

  // Breathing time for the loop
  delay(BREATHING_DELAY_MILLISEC);
}
