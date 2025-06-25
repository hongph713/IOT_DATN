#include <HTTPUpdate.h>
WiFiClient netClient;
void update_started() {
  Serial.println("CALLBACK:  HTTP update process started");
}

void update_finished() {
  Serial.println("CALLBACK:  HTTP update process finished");
}

void update_progress(int cur, int total) {
  Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err) {
  Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}

void ota_proc(void) {
 if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[OTA]-Network ready to OTA");

    // WiFiClient netClient;

    httpUpdate.onStart(update_started);
    httpUpdate.onEnd(update_finished);
    httpUpdate.onProgress(update_progress);
    httpUpdate.onError(update_error);

    httpUpdate.rebootOnUpdate(false); // remove automatic reboot

    Serial.println(F("Update start now!"));

    char HTTP_UPDATE[100];
    sprintf(HTTP_UPDATE, "http://cloud.longhungtech.com/Test_firmware_%s", SYS_ChipID);
    Serial.println(HTTP_UPDATE);
    t_httpUpdate_return ret = httpUpdate.update(netClient, String(HTTP_UPDATE), FIRMWARE_VERSION);

    switch (ret) {
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
        break;
      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        break;
      case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        delay(1000);
        ESP.restart();
        break;
    }
  } else {
    Serial.println("[OTA]-Retry connect Internet Network");
    WiFi.reconnect();
    Serial.println("No Internet connect to Update OTA!");
  }
}

void startCFOTA() {
  String url = "http://cloud.longhungtech.com/Petrol_reader_CCDBA7FA3A3C?version=" + String(FIRMWARE_VERSION);
  String command = "AT+CFOTA=0,1," + url + ",simcom,simcom";

  Serial2.println(command);  // Gửi lệnh AT tới SIM module
  Serial.println("Gửi lệnh: " + command);

  unsigned long timeout = millis() + 60000; // Cho phép 60 giây tải firmware
  String response = "";
  bool fotaStarted = false;

  while (millis() < timeout) {
    int percent;
    while (Serial2.available()) {
      char c = Serial2.read();
      response += c;

      // In ra từng ký tự nếu muốn theo dõi chi tiết
      Serial.print(c);

      // Kiểm tra nếu bắt đầu FOTA
      if (response.indexOf("+CFOTA: FOTA,START") != -1 && !fotaStarted) {
        Serial.println("\nFOTA started...");
        ESP.restart();
        fotaStarted = true;
        response = "";  // reset để tiếp tục theo dõi tiến trình
      }

      // Nếu có lỗi
      if (response.indexOf("ERROR") != -1) {
        Serial.println("\nLỗi khi thực hiện CFOTA: " + response);
        return;
      }

      // Nếu có kết quả CFOTA khác
      if (response.indexOf("+CFOTA:") != -1 && response.indexOf("FOTA,START") == -1 && response.indexOf("DOWNLOADING") == -1) {
        Serial.println("\nPhản hồi CFOTA khác: " + response);
        response = "";
      }
    }

    // Thêm điều kiện thoát vòng ngoài nếu cần
    if (percent >= 100 || response.indexOf("ERROR") != -1) {
      break;
    }
    delay(10); // tránh chiếm CPU 100%
  }

  Serial.println("Kết thúc hoặc hết thời gian chờ.");
}
