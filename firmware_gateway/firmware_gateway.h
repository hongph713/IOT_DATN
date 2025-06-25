#include <WiFi.h>
#include <ArduinoJson.h>
#include "time.h"
#include <EEPROM.h>
#include "HardwareSerial.h"

char SYS_HostName[20];
#define SIM7670Serial Serial2

#define FIRMWARE_VERSION    "1"

#define EEPROM_SIZE         512
#define TOTAL_CLIENT        3

// WiFi credentials
// const char* ssid = "LoH Tech 2_4G";
// const char* password = "13579LOH";

const char* ssid = "TP-Link";
const char* password = "18438147";