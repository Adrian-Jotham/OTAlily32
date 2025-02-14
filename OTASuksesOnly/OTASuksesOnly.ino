#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>  // Include ArduinoJson library

const char* ssid = "keenos";
const char* password = "09871234";
const char* mqtt_server = "192.168.1.247";
const char amazon_root_ca[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----

)EOF";

WiFiClient espClient;
PubSubClient client(espClient);

const char* device_id = "3855";
const char* ota_topic = "devices/ota/update/3855";
const char* ota_status_topic = "devices/ota/status/3855";

void callback(char* topic, byte* payload, unsigned int length) {
    String message = "";
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    if (String(topic) == ota_topic) {
        Serial.println("Received OTA update command: " + message);
        perform_ota_update(message);
    }
}

void perform_ota_update(String payload) {
    // Parse the JSON message to extract the firmware URL
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.print("Failed to parse JSON: ");
        Serial.println(error.f_str());
        return;
    }
    
    // Extract the firmware URL from JSON
    String firmware_url = doc["firmware_url"].as<String>();
    Serial.println("Firmware URL: " + firmware_url);

    WiFiClientSecure httpsClient;
    HTTPClient https;

    // Disable SSL certificate verification (Not recommended for production!)
    // httpsClient.setCACert(amazon_root_ca);
    httpsClient.setInsecure(); 

    Serial.println("Starting OTA update...");
    https.begin(httpsClient, firmware_url);  // Use WiFiClientSecure
    Serial.println(firmware_url);
    https.setTimeout(15000);

    int httpCode = https.GET();
    if (httpCode == HTTP_CODE_OK) {
        int contentLength = https.getSize();
        bool canBegin = Update.begin(contentLength);

        if (canBegin) {
            Serial.println("Flashing firmware...");
            WiFiClient* stream = https.getStreamPtr();
            size_t written = Update.writeStream(*stream);

            if (written == contentLength) {
                Serial.println("Firmware written successfully!");
            } else {
                Serial.printf("Written %d / %d bytes. OTA Failed!\n", written, contentLength);
            }

            if (Update.end()) {
                Serial.println("Update finished. Rebooting...");
                ESP.restart();
            } else {
                Serial.println("Update failed!");
            }
        }
    } else {
        Serial.printf("Failed to download firmware. HTTP Response Code: %d\n", httpCode);
    }
    https.end();
}

void setup() {
    Serial.begin(115200);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
    
    while (!client.connected()) {
        client.connect(device_id);
    }

    client.subscribe(ota_topic);
}

void loop() {
    client.loop();
}
