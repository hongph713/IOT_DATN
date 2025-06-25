
String response = "";
void sendATCommand(String cmd, String expectedResponse, int timeout) {
  SIM7670Serial.println(cmd);
  response = "";
  long timeStart = millis();
  while (millis() - timeStart < timeout) {
    while (SIM7670Serial.available() > 0) {
      char c = SIM7670Serial.read();
      response += c;
    }
    delay(100);
    if (response.indexOf(expectedResponse) != -1) {
      break;
    }   
  }

  Serial.println(response);
}

//init 4G
void init_4G(){
  SIM7670Serial.begin(115200, SERIAL_8N1, 32, 33);
  // sendATCommand("AT+CRESET", "OK", 5000); // check communication
  // // SIM7670Serial.println("AT+CFUN=1");
  // delay(10000);

  sendATCommand("AT", "OK", 500); // check communication
  //  delay(6000);
  sendATCommand("AT+CMGF=1", "OK", 500); // set SMS format to text
  // sendATCommand("AT+CMQTTSTOP", "OK", 5000);
  //  delay(6000);
  // sendATCommand("AT+CFUN=1,1", "OK", 10000);
  sendATCommand("AT+CGATT=1", "OK", 500);
  sendATCommand("AT+CGDCONT=1,\"IP\",\"v-internet\"", "OK", 500);
  sendATCommand("AT+CGACT=1,1", "OK", 500);
  sendATCommand("AT+CMQTTSTOP", "OK", 500);
}

void sendDeviceToFirebase(const Device& device) {
  DynamicJsonDocument doc(512);
  doc["temp"] = device.temp;
  doc["humidity"] = device.humid;
  doc["pm25"] = device.pm;
  JsonObject timeObj = doc.createNestedObject("time");
  timeObj[".sv"] = "timestamp";

  String jsonData;
  serializeJson(doc, jsonData); // jsonData sẽ chứa chuỗi JSON cần gửi
  String url = String(firebaseHost) + "/cacThietBiQuanTrac/" + String(device.uid_str) + ".json";

  // Khởi tạo HTTP
  sendATCommand("AT+HTTPTERM", "OK", 200);         // Luôn nên gọi trước để tránh lỗi session cũ
  sendATCommand("AT+HTTPINIT", "OK", 200);

  // Cấu hình địa chỉ URL Firebase
  sendATCommand("AT+HTTPPARA=\"URL\",\"" + url + "\"", "OK", 200);

  // Dùng HTTPS
  sendATCommand("AT+HTTPPARA=\"CID\",1", "OK", 200);
  sendATCommand("AT+HTTPSSL=1", "OK", 200);

  // Đặt header content-type JSON
  sendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK", 200);

  // Gửi dữ liệu JSON
  int len = jsonData.length();
  sendATCommand("AT+HTTPDATA=" + String(len) + ",1000", "DOWNLOAD", 2000);
  sendATCommand(jsonData, "OK", 500);

  // Gửi POST request
  sendATCommand("AT+HTTPACTION=1", "+HTTPACTION: 1,", 500); // Sau đó check mã HTTP

  // Đọc phản hồi từ Firebase
  sendATCommand("AT+HTTPREAD", "OK", 500);
  sendATCommand("AT+HTTPTERM", "OK", 200); // Kết thúc session

  // WiFiClientSecure client;
  // client.setInsecure();

  // HTTPClient http;

  // String url = String(firebaseHost) + "/cacThietBiQuanTrac/" + String(device.uid_str) + ".json";
  // http.begin(client, url);
  // http.addHeader("Content-Type", "application/json");

  // DynamicJsonDocument doc(512);
  // doc["temp"] = device.temp;
  // doc["humidity"] = device.humid;
  // doc["pm25"] = device.pm;

  // // Trường time được Firebase tự sinh timestamp server
  // JsonObject timeObj = doc.createNestedObject("time");
  // timeObj[".sv"] = "timestamp";

  // String jsonData;
  // serializeJson(doc, jsonData);

  // Serial.println("Sending to: " + url);
  // Serial.println(jsonData);

  // int code = http.POST(jsonData);

  // Serial.print("HTTP Response: ");
  // Serial.println(code);

  // if (code > 0) {
  //   String payload = http.getString();
  //   Serial.println("Firebase Response:");
  //   Serial.println(payload);
  // } else {
  //   Serial.println("Failed to send data");
  // }

  // http.end();
}