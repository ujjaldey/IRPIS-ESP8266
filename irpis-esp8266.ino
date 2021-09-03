#include <ESP8266WiFi.h>

#define DEBUG_OUTPUT 0

// Set the WIFI SSID and password
// Replace with your SSID and password
const char* wifiSSID = "BongConnectionN";
const char* wifiPassword = "Bong@123";
//const char* wifiSSID = "<your wifi ssid>";
//const char* wifiPassword = "<your wifi password>";


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
  Serial.print("Connecting to ");
  Serial.println(wifiSSID);

  // Wait for the connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected to ");
  Serial.println(wifiSSID);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("");
}

void setup() {
  // Set the BAUD rate to 115200
  Serial.begin(115200);
  // Set the debug output mode
  Serial.setDebugOutput(DEBUG_OUTPUT);

  // Setup the WiFi parameters and connect to it
  initWiFi();
}

void loop() {
  // put your main code here, to run repeatedly:
}
