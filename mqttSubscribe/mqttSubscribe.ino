#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

// WiFi Credentials
const char* ssid = "keenos";
const char* password = "09871234";
const char* dev_id = "2855";  // Hardcoded device ID 2855/3186

// MQTT Broker
const char* mqtt_server = "192.168.1.247";
const int mqtt_port = 1883;
const String mqtt_sensor_topic = "device/" + String(dev_id) + "/sensors";
const String mqtt_status_topic = "device/" + String(dev_id) + "/status";
const String mqtt_control_topic = "device/" + String(dev_id) + "/cmd";  // New topic for LED control
const String firmware_version = "1.2.3";

WiFiClient espClient;
PubSubClient client(espClient);

// OLED Display Setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
#define OLED_I2C_ADDRESS 0x3C

// Built-in LED (Change GPIO if needed)
#define BUILTIN_LED 25

// Packet Counters
int packetsSent = 0;
int packetsFailed = 0;

void displayMessage(String message, int x, int y, uint16_t color = WHITE) {
  display.setTextColor(color);
  display.setCursor(x, y);
  display.println(message);
}

// WiFi Connection Setup
void setup_wifi() {
  display.clearDisplay();
  displayMessage("Connecting to WiFi...", 10, 20);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  displayMessage("WiFi Connected!", 10, 40);
  displayMessage("IP: " + WiFi.localIP().toString(), 10, 50);
  display.display();
}

// MQTT Callback Function
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received on topic: ");
  Serial.println(topic);

  StaticJsonDocument<256> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, payload, length);
  
  if (error) {
    Serial.println("Failed to parse JSON");
    return;
  }

  if (jsonDoc.containsKey("cmd") && jsonDoc.containsKey("target") && jsonDoc.containsKey("parameter")) {
    String cmd = jsonDoc["cmd"].as<String>();
    String target = jsonDoc["target"].as<String>();
    int parameter = jsonDoc["parameter"].as<int>();

    if (target == "built-in led") {
      if (cmd == "test" && parameter == 1) {
        digitalWrite(BUILTIN_LED, HIGH);
        Serial.println("LED turned ON");
      } else if (cmd == "test" && parameter == 0) {
        digitalWrite(BUILTIN_LED, LOW);
        Serial.println("LED turned OFF");
      }
    }
  }
}

// MQTT Reconnection
void reconnect() {
  String clientId = "ESP32Client_" + String(dev_id);
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection... ");
    if (client.connect(clientId.c_str())) {
      Serial.println("Connected to MQTT!");
      client.subscribe(mqtt_control_topic.c_str());  // Subscribe to control topic
      displayMessage("MQTT Connected!", 10, 40);
    } else {
      Serial.print("MQTT failed, error code: ");
      Serial.println(client.state());
      displayMessage("MQTT Failed!", 10, 40);
      packetsFailed++;
      delay(5000);
    }
  }
}

// Publish Sensor Data
void publish_sensor_data() {
  float temperature = random(1500, 3000) / 100.0;
  float humidity = random(3000, 7000) / 100.0;

  StaticJsonDocument<128> jsonDoc;
  jsonDoc["device_id"] = dev_id;
  jsonDoc["temperature"] = temperature;
  jsonDoc["humidity"] = humidity;
  jsonDoc["timestamp"] = millis();

  char buffer[256];
  serializeJson(jsonDoc, buffer);

  if (client.publish(mqtt_sensor_topic.c_str(), buffer, false)) {
    packetsSent++;
  } else {
    packetsFailed++;
  }

  display.clearDisplay();
  displayMessage("Temp: " + String(temperature) + " C", 10, 0);
  displayMessage("Humidity: " + String(humidity) + " %", 10, 10);
  displayMessage("Device ID: " + String(dev_id), 10, 30);
  displayMessage("Packets Sent: " + String(packetsSent), 10, 40);
  displayMessage("Packets Failed: " + String(packetsFailed), 10, 50);
  display.display();
}

// Publish Status Data
void publish_status_data() {
  unsigned long uptime = millis() / 1000;
  int rssi_level = WiFi.RSSI();
  int battery_level = 95;  // Example static battery level

  StaticJsonDocument<256> jsonDoc;
  jsonDoc["status"] = "online";
  jsonDoc["uptime"] = uptime;
  jsonDoc["firmware_version"] = firmware_version;
  jsonDoc["rssi_level"] = rssi_level;
  jsonDoc["battery_level"] = battery_level;
  jsonDoc["device_id"] = dev_id;
  jsonDoc["timestamp"] = millis();

  char buffer[256];
  serializeJson(jsonDoc, buffer);

  if (client.publish(mqtt_status_topic.c_str(), buffer, false)) {
    packetsSent++;
  } else {
    packetsFailed++;
  }

  display.clearDisplay();
  displayMessage("RSSI: " + String(rssi_level) + " dBm", 10, 0);
  displayMessage("Battery: " + String(battery_level) + " %", 10, 10);
  displayMessage("Uptime: " + String(uptime) + "s", 10, 20);
  displayMessage("Device ID: " + String(dev_id), 10, 30);
  displayMessage("Packets Sent: " + String(packetsSent), 10, 40);
  displayMessage("Packets Failed: " + String(packetsFailed), 10, 50);
  display.display();
}

void setup() {
  Serial.begin(115200);
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, LOW);  // Ensure LED is off initially

  display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS, SDA, SCL);
  display.display();
  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqtt_callback);
  client.setKeepAlive(60);
  delay(2000);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  publish_sensor_data();
  publish_status_data();
  
  delay(5000);
}
