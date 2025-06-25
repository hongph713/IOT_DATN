#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "time.h"

// WiFi credentials
const char* ssid = "QUANG_DAM";
const char* password = "30111971";

// Firebase Database
const char* firebaseHost = "https://rapid-stream-460402-c3-default-rtdb.asia-southeast1.firebasedatabase.app";

// NTP Server
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600;
const int daylightOffset_sec = 0;

// Gửi mỗi 30 giây
unsigned long lastSend = 0;
const unsigned long sendInterval = 30000;

// Cấu trúc thiết bị
struct Device {
  const char* id;
  double lat;
  double lon;
  float temp;
  float humidity;
  float pm25;
};

// 3 thiết bị giả lập
Device devices[] = {
  {"id_1_caugiay",    21.0315, 105.782, 30.0, 60.0, 28.1},
  {"id_2_hoankiem",   21.0515, 105.882, 30.0, 60.0, 40.0},
  {"id_3_haibatrung", 21.0415, 105.712, 30.0, 60.0, 150.0}
};

void sendDeviceToFirebase(const Device& device) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  String url = String(firebaseHost) + "/cacThietBiQuanTrac/" + device.id + ".json";
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(512);
  doc["temp"] = device.temp;
  doc["humidity"] = device.humidity;
  doc["pm25"] = device.pm25;
  doc["lat"] = device.lat;
  doc["long"] = device.lon;

  // Trường time được Firebase tự sinh timestamp server
  JsonObject timeObj = doc.createNestedObject("time");
  timeObj[".sv"] = "timestamp";

  String jsonData;
  serializeJson(doc, jsonData);

  Serial.println("Sending to: " + url);
  Serial.println(jsonData);

  int code = http.POST(jsonData);

  Serial.print("HTTP Response: ");
  Serial.println(code);

  if (code > 0) {
    String payload = http.getString();
    Serial.println("Firebase Response:");
    Serial.println(payload);
  } else {
    Serial.println("Failed to send data");
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Syncing NTP time...");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nNTP time synced.");
}

void loop() {
  if (millis() - lastSend > sendInterval) {
    for (int i = 0; i < 3; i++) {
      sendDeviceToFirebase(devices[i]);
    }
    lastSend = millis();
  }
}
