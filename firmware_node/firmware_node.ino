#define RH_RF95_MAX_MESSAGE_LEN 50
#include "DHT.h"
#include "stdint.h"
#include <EEPROM.h>
#include <SPI.h>
#include <RH_RF95.h>
#include <RHMesh.h>

//PM
#define LED_PIN    21       // GPIO để bật/tắt LED phát trong cảm biến
#define ANALOG_PIN 34      // GPIO có chức năng ADC

//DHT11
#define DHTPIN 17        // GPIO17
#define DHTTYPE DHT11    // DHT 11
DHT dht(DHTPIN, DHTTYPE);

//Lora
#define EEPROM_SIZE     512
#define MOSI            23
#define MISO            19
#define SCK             18
#define SS              5
#define DIO             26

#define CLIENT_ADDRESS  255
#define LORA_FREQ       434.0

RH_RF95 rf95(SS, DIO);
RHMesh *manager;

char SYS_ChipID[12];
uint8_t node_dev_uid[6];

uint8_t node_dev_address = 255;
uint8_t join_stt;
float temp, humid, pm;
float voltage;

void setup() 
{
  Serial.begin(115200);
  while (!Serial) ; // Wait for serial port to be available
  setDeviceID();    //read uid
  if (!EEPROM.begin(EEPROM_SIZE)) { //Init EEPROM
    Serial.println("failed to init EEPROM");
  }
  node_dev_address = EEPROM.read(0);
  Serial.printf("node_dev_address = %d\n",node_dev_address);
  join_stt = EEPROM.read(1);
  Serial.printf("join_stt = %d\n",join_stt);
  
  //init PM
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  //init PM
  dht.begin();

  //init Lora
  SPI.begin(SCK, MISO, MOSI, SS);
  manager = new RHMesh(rf95, node_dev_address);
  
  if (!manager->init())
    Serial.println("init failed");
  else
    Serial.println("init client sucess!");

  rf95.setFrequency(LORA_FREQ);
  rf95.setTxPower(22);
}

uint8_t data_send[RH_RF95_MAX_MESSAGE_LEN];
uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t len = sizeof(buf);
uint8_t from;
uint8_t uid[6];
uint8_t count = 0;
uint8_t address = 255;

uint32_t lora_event = 0;
const unsigned long lora_interval = 5000UL; // 5s
void loop()
{  
  if(millis() < lora_event) {
    lora_event = millis();
  }  
  if (millis() - lora_event >= lora_interval) {
    lora_event = millis();
    
    //PM
    digitalWrite(LED_PIN, HIGH);   // Bật LED (theo datasheet)
    delayMicroseconds(280);       // Chờ ổn định
    int analogValue = analogRead(ANALOG_PIN);
    delayMicroseconds(40);
    digitalWrite(LED_PIN, LOW);  // Tắt LED
    delayMicroseconds(9680);      // Chờ cho chu kỳ 10ms
    // Chuyển đổi giá trị ADC sang điện áp
    voltage = analogValue * (3.3 / 4095.0);  // ADC 12-bit
    // Tính toán nồng độ bụi (theo datasheet của Sharp)
    pm = (voltage - 0.6) * 0.5 * 1000;  // đơn vị: µg/m³
    if (pm < 0) pm = 0;
    Serial.print("ADC: ");
    Serial.print(analogValue);
    Serial.print(" | Voltage: ");
    Serial.print(voltage, 3);
    Serial.print(" V | Dust: ");
    Serial.print(pm, 1);
    Serial.println(" µg/m³");

    //Read temperature and humidity
    humid = dht.readHumidity();
    temp = dht.readTemperature(); // Mặc định là Celsius

    // Kiểm tra lỗi
    if (isnan(humid) || isnan(temp)) {
      Serial.println(F("Failed to read from DHT sensor!"));
      return;
    }

    Serial.print(F("Humidity: "));
    Serial.print(humid);
    Serial.print(F("%  Temperature: "));
    Serial.print(temp);
    Serial.println(F("°C"));
  }

  //Send data to Gateway
  Serial.println("Sending to client");
  if (manager->recvfromAckTimeout(buf, &len, 2000, &from))
  {        
    if(join_stt != 1)
    {
      for (int i = 0; i < 16; i++) 
      {
          Serial.printf("%d ",buf[i]);
      }
      Serial.println();

      uid[0] = buf[0];
      uid[1] = buf[1];
      uid[2] = buf[2];
      uid[3] = buf[3];
      uid[4] = buf[4];
      uid[5] = buf[5];
      address = buf[7];    
      Serial.printf("Receive to Node success %d\n", address);
      for(uint8_t i = 0; i < 6; i++)
      {
        if(uid[i] == node_dev_uid[i])
        {
          count++;
          Serial.printf("count = %d", count);
          Serial.println();
          Serial.printf("uid[%d] = ", i);
          Serial.println(uid[i]);
        }
        else
        {
          count = 0;
        }
      }
      if(count == 6)
      {
        node_dev_address = address;
        join_stt = 1;
        Serial.print("node_dev_address = ");
        Serial.println(node_dev_address);
        EEPROM.write(0, node_dev_address);
        EEPROM.write(1, join_stt);
        EEPROM.commit();
      }
      else
      {
        join_stt = 0;
      }

      data_send[0] = node_dev_uid[0];
      data_send[1] = node_dev_uid[1];
      data_send[2] = node_dev_uid[2];
      data_send[3] = node_dev_uid[3];
      data_send[4] = node_dev_uid[4];
      data_send[5] = node_dev_uid[5];
      data_send[6] = random(255);
      data_send[7] = node_dev_address;
      data_send[8] = join_stt;
      if(manager->sendtoWait(data_send, sizeof(data_send), from) == RH_ROUTER_ERROR_NONE)
      {
        Serial.println("sendtoWait success");  
        ESP.restart();
      }
      else
      {
        Serial.println("sendtoWait failed");
      }
    }
    else if(join_stt == 1)
    {
      Serial.print("got reply: ");
      Serial.println((char*)buf);
      Serial.print("RSSI: ");
      Serial.println(rf95.lastRssi(), DEC);    

      data_send[0] = temp / 100;
      data_send[1] = (uint8_t)temp % 100;
      data_send[2] = humid / 10;
      data_send[3] = (uint8_t)humid % 100;
      data_send[4] = pm / 10;
      data_send[5] = (uint8_t)pm % 100;
      data_send[6] = random(255);
      data_send[7] = node_dev_address;
      data_send[8] = join_stt;
      Serial.print("data_send to gateway: ");
      for(uint8_t i = 0; i < 8; i++){
        Serial.print(data_send[i]);
        Serial.print(" ");
      }
      Serial.println();
      if(manager->sendtoWait(data_send, sizeof(data_send), from) == RH_ROUTER_ERROR_NONE)
      {
        Serial.println("sendtoWait success");  
      }
      else
      {
        Serial.println("sendtoWait failed");
      }
    }
  }
  else
  {
    Serial.println("recv failed");
  }
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
  char str[2];
  for(uint8_t i = 0; i < 6; i++)
  {
    strncpy(str, SYS_ChipID + (i * 2), 2);
    node_dev_uid[i]=hexChar2Decimal(str);
    Serial.printf("node_dev.uid[%d] = ", i);
    Serial.println(node_dev_uid[i]);
  }
}

// To convert a hex value in string to decimal
int hexChar2Decimal(const char* hexInput)
{
  return strtol(hexInput, NULL, 16);
}