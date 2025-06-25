#include "firmware_gateway.h"
// Firebase Database
const char* firebaseHost = "https://rapid-stream-460402-c3-default-rtdb.asia-southeast1.firebasedatabase.app";

// NTP Server
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600;
const int daylightOffset_sec = 0;

// Gửi mỗi 30 giây
unsigned long lastSend = 0;
const unsigned long sendInterval = 30000;

struct Device 
{
  uint8_t uid[6];
  char uid_str[13];
  uint8_t address;
  uint8_t join_stt;
  uint16_t temp;
  uint16_t humid;
  uint16_t pm;
};

Device node_dev[TOTAL_CLIENT] = {};

char SYS_ChipID[13];
void setup() 
{
  Serial.begin(115200);
  node_dev[0].uid[0] = 188;
  node_dev[0].uid[1] = 221;
  node_dev[0].uid[2] = 194;
  node_dev[0].uid[3] = 205;
  node_dev[0].uid[4] = 163;
  node_dev[0].uid[5] = 216;
  // Tạo chuỗi hex
  snprintf(node_dev[0].uid_str, sizeof(node_dev[0].uid_str), "%02X%02X%02X%02X%02X%02X",
           node_dev[0].uid[0],
           node_dev[0].uid[1],
           node_dev[0].uid[2],
           node_dev[0].uid[3],
           node_dev[0].uid[4],
           node_dev[0].uid[5]);

  Serial.print("UID0 Hex: ");
  Serial.println(node_dev[0].uid_str);

  node_dev[1].uid[0] = 20;
  node_dev[1].uid[1] = 43;
  node_dev[1].uid[2] = 47;
  node_dev[1].uid[3] = 192;
  node_dev[1].uid[4] = 67;
  node_dev[1].uid[5] = 96;
  // Tạo chuỗi hex
  snprintf(node_dev[1].uid_str, sizeof(node_dev[1].uid_str), "%02X%02X%02X%02X%02X%02X",
           node_dev[1].uid[0],
           node_dev[1].uid[1],
           node_dev[1].uid[2],
           node_dev[1].uid[3],
           node_dev[1].uid[4],
           node_dev[1].uid[5]);
  Serial.print("UID1 Hex: ");
  Serial.println(node_dev[1].uid_str);

  node_dev[2].uid[0] = 188;
  node_dev[2].uid[1] = 221;
  node_dev[2].uid[2] = 194;
  node_dev[2].uid[3] = 206;
  node_dev[2].uid[4] = 19;
  node_dev[2].uid[5] = 28;
  // Tạo chuỗi hex
  snprintf(node_dev[2].uid_str, sizeof(node_dev[2].uid_str), "%02X%02X%02X%02X%02X%02X",
           node_dev[2].uid[0],
           node_dev[2].uid[1],
           node_dev[2].uid[2],
           node_dev[2].uid[3],
           node_dev[2].uid[4],
           node_dev[2].uid[5]);

  Serial.print("UID2 Hex: ");
  Serial.println(node_dev[2].uid_str);

  Serial.println("\r\n[MAIN]-Welcome to Firmware");
  Serial.print("[MAIN]-FIRMWARE VERSION:");
  Serial.println(FIRMWARE_VERSION);

  WiFi.setHostname(SYS_HostName);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false); // Do not sleep
  Serial.println("Connecting to WiFi...");

  unsigned long startAttemptTime = millis();
  //init wifi
  while (WiFi.status() != WL_CONNECTED) {  
    if (millis() - startAttemptTime > 5000) {
      // Quá 5 giây, thoát vòng lặp
      Serial.println("WiFi connection timed out.");
      break;
    }
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
  //read UID Gateway
  setDeviceID();
  delay(1000);
  //check update OTA
  ota_proc();   //update OTA via WIFI
  init_4G();    //init 4G

  // //Time
  // configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  // Serial.println("Syncing NTP time...");
  // struct tm timeinfo;
  // while (!getLocalTime(&timeinfo)) {
  //   delay(500);
  //   Serial.print(".");
  // }
  // Serial.println("\nNTP time synced.");

  //Init EEPROM
  if (!EEPROM.begin(EEPROM_SIZE)) { 
    Serial.println("failed to init EEPROM");
  }

  // for(uint16_t i = 0; i < EEPROM_SIZE; i++){
  //   EEPROM.write(i, 255);
  // }
  // EEPROM.commit();

  for(uint8_t i = 0; i < TOTAL_CLIENT; i++){
    node_dev[i].join_stt = EEPROM.read(i);
  }

  init_lora();  //init lora
}
 
uint32_t synTime_event = 0;
const unsigned long time_interval = 30000UL; // 30 phút = 30 * 60 * 1000 = 1,800,000 ms

uint32_t lora_event = 0;
const unsigned long lora_interval = 5000UL; // 5s

uint32_t ota_event = 0;
const unsigned long ota_interval = 600000UL;
void loop()
{
  unsigned long currentMillis = millis();

  // Reset if millis() overflows (approximately every ~50 days)
  if (currentMillis < synTime_event) 
    synTime_event = currentMillis;
  if (currentMillis < ota_event) 
    ota_event = currentMillis;
  if (currentMillis < lora_event) 
    lora_event = currentMillis;

  // Synchronize time every 30 minutes
  if (currentMillis - synTime_event >= time_interval) {
    synTime_event = currentMillis;
    broadcast_network(); // Send BROADCAST address to join the network
  }

  // Check OTA every 10 minutes
  if (currentMillis - ota_event >= ota_interval) {
    ota_event = currentMillis;
    startCFOTA(); // Check ESP32 version via 4G web; if outdated, ESP32 resets
  }

  // Update LoRa every 5 seconds
  if (currentMillis - lora_event >= lora_interval) {
    lora_event = currentMillis;
    update_node(); // Update Node data, then send data to app
  }

  // No delay to keep real-time responsiveness
}

void setDeviceID()
{
  uint8_t chipid[6];
  esp_efuse_mac_get_default(chipid);

  // Format into uppercase mac address without :
  // Eg: BCDDC2C31684
  sprintf(SYS_ChipID, "%02X%02X%02X%02X%02X%02X",chipid[0], chipid[1], chipid[2], 
            chipid[3], chipid[4], chipid[5]);
  // strncpy(uid, SYS_ChipID, 12);
  Serial.print("[UTILS] ChipID: ");
  Serial.println(SYS_ChipID);
}

// To convert a hex value in string to decimal
int hexChar2Decimal(const char* hexInput)
{
  return strtol(hexInput, NULL, 16);
}

// ====================================
// Converter
// ====================================
char* string2Char(String str){
    if(str.length()!=0){
        char *p = const_cast<char*>(str.c_str());
        return p;
    }
    return 0;
}
