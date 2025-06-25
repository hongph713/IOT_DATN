#define RH_RF95_MAX_MESSAGE_LEN 50
#include <SPI.h>
#include <RH_RF95.h>
#include <RHMesh.h>

#define SERVER_ADDRESS      252
#define BROADCAST_ADDRESS   255
#define LORA_FREQ           434.0
#define MOSI                23
#define MISO                19
#define SCK                 18
#define SS                  5
#define DIO                 26

RH_RF95 rf95(SS, DIO);
RHMesh manager(rf95, SERVER_ADDRESS);
//Init Lora
void init_lora(void) 
{
  //setup SPI
  SPI.begin(SCK, MISO, MOSI, SS);

  //setup Lora
  //different gateway use different frequency channel
  // rf95_1.setFrequency(434.0);
  if (!manager.init())
  {
    Serial.println("[LORA1]-RF95 init failed");
  }

  rf95.setFrequency(LORA_FREQ);
  rf95.setTxPower(22);

  for(uint8_t i = 0; i < TOTAL_CLIENT; i++){
    node_dev[i].address = i;
  }
  Serial.printf("[LORA]-Init Lora frequency %d\n", LORA_FREQ);
}

uint8_t data_send[RH_RF95_MAX_MESSAGE_LEN];
uint8_t data_recv[RH_RF95_MAX_MESSAGE_LEN];
uint8_t request_network = 0;
uint8_t len_broadcast_network = sizeof(data_recv); 
uint8_t from_broadcast_network;
void broadcast_network(void) 
{
  Serial.println("[G2N]-broadcast_network");

  if(node_dev[request_network].join_stt != 1)
  {
    data_send[0] = node_dev[request_network].uid[0];
    data_send[1] = node_dev[request_network].uid[1];
    data_send[2] = node_dev[request_network].uid[2];
    data_send[3] = node_dev[request_network].uid[3];
    data_send[4] = node_dev[request_network].uid[4];
    data_send[5] = node_dev[request_network].uid[5];
    data_send[6] = random(255);
    data_send[7] = node_dev[request_network].address;

    if (manager.sendtoWait(data_send, sizeof(data_send), BROADCAST_ADDRESS) == RH_ROUTER_ERROR_NONE)
    {
      Serial.printf("[G2N]-Send to Node %d success", request_network);
      Serial.println();
      if (manager.recvfromAckTimeout(data_recv, &len_broadcast_network, 2000, &from_broadcast_network))
      {
        uint8_t address = data_recv[7];
        node_dev[request_network].join_stt = data_recv[8];
        Serial.printf("[G2N]-Receive to Node %d success, join_stt = %d", address, node_dev[request_network].join_stt);
        Serial.println();
        EEPROM.write(request_network, node_dev[request_network].join_stt);
        EEPROM.commit();
      }
      else
      {
        Serial.println("No reply received from client");
      }
    }
    else
    {
      Serial.printf("[G2N]-Send to Node %d failed %d", BROADCAST_ADDRESS, request_network);
      Serial.println();
    }
    request_network++;
    if(request_network >= TOTAL_CLIENT)
    {
      request_network = 0;
    }
  }
}

uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t len = sizeof(buf);
uint8_t from;
void update_node(){
  Serial.println("ping client");
  for(uint8_t i = 0; i < TOTAL_CLIENT; i++)
  {
    data_send[0] = node_dev[i].uid[0];
    data_send[1] = node_dev[i].uid[1];
    data_send[2] = node_dev[i].uid[2];
    data_send[3] = node_dev[i].uid[3];
    data_send[4] = node_dev[i].uid[4];
    data_send[5] = node_dev[i].uid[5];
    data_send[6] = random(255);
    data_send[7] = node_dev[i].address;
    uint8_t er_flag = manager.sendtoWait(data_send, sizeof(data_send), i);
    if (er_flag == RH_ROUTER_ERROR_NONE)
    {
      Serial.printf("Send to address to %d\n", i);
      if(manager.recvfromAckTimeout(buf, &len, 2000, &from))
      {
        Serial.print("data receive from node: ");
        for(uint8_t i = 0; i < len; i++){
          Serial.print(buf[i]);
          Serial.print(" ");
        }
        Serial.println();
        Serial.print("RSSI: ");
        Serial.println(rf95.lastRssi(), DEC);
        node_dev[i].temp = buf[1]*100;
        node_dev[i].temp |= buf[0];
        Serial.printf("node_dev[%d].temp = %d\n", i, node_dev[i].temp);
        node_dev[i].humid = buf[3]*100;
        node_dev[i].humid |= buf[2];
        Serial.printf("node_dev[%d].humid = %d\n", i, node_dev[i].humid);
        node_dev[i].pm = buf[5]*100;
        node_dev[i].pm |= buf[4];
        Serial.printf("node_dev[%d].pm = %d\n", i, node_dev[i].pm);
        sendDeviceToFirebase(node_dev[i]);
      }
      else
      {
        Serial.println("recv failed");
      }
    }
    else
    {
      Serial.printf("sendtoWait failed to %d, INDEX:", i);
      Serial.println(er_flag,DEC);
    }
    delay(1000);
  }
}