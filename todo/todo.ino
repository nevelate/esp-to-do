#include <Arduino.h>
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include <LittleFS.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <AsyncMessagePack.h>

#include <U8g2lib.h>
#include <Wire.h>
#include <MUIU8g2.h>

#include "bitmaps.h"
#include "constants.h"

AsyncWebServer server(80);
#include <EncButton.h>
EncButton encoder(ENC_A, ENC_B, ENC_SWITCH);

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE, /* clock=*/I2C_SCL, /* data=*/I2C_SDA);  // ESP32 Thing, HW I2C with pin remapping

int selectedTaskIndex = 0, lowerBoundTaskIndex = 3;
int lastEncoderCounter;
long lastUpdate;

JsonDocument doc;
JsonArray tasks;

bool isWiFiEnabled = false;

void setup() {
  if (!LittleFS.begin()) return;

  esp_deep_sleep_enable_gpio_wakeup(BIT(ENC_SWITCH), ESP_GPIO_WAKEUP_GPIO_LOW);

  u8g2.begin();
  u8g2.setPowerSave(0);

  //LittleFS.format();

  lastUpdate = millis();

  loadTasks();
  drawTodos();
}

void loop() {
  encoder.tick();
  if (encoder.click() && !isWiFiEnabled) {
    lastUpdate = millis();
    tasks[selectedTaskIndex]["completed"] = !tasks[selectedTaskIndex]["completed"];
    saveTasks();
    drawTodos();
  }
  if (encoder.turn() && !isWiFiEnabled) {
    lastUpdate = millis();
    selectedTaskIndex += encoder.dir();
    if (selectedTaskIndex <= lowerBoundTaskIndex - 4) lowerBoundTaskIndex--;
    selectedTaskIndex = constrain(selectedTaskIndex, 0, tasks.size() - 1);    
    lowerBoundTaskIndex = constrain(lowerBoundTaskIndex, 3, tasks.size() - 1);
    if (selectedTaskIndex > lowerBoundTaskIndex) lowerBoundTaskIndex++;
    drawTodos();
  }
  if (encoder.hold()) {
    changeServerState();
    isWiFiEnabled = !isWiFiEnabled;
  }
  if (millis() - lastUpdate > SLEEP_DELAY && !isWiFiEnabled) {
    u8g2.setPowerSave(1);
    esp_deep_sleep_start();
  }
}

void drawTodos() {
  u8g2.clearBuffer();

  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_haxrcorp4089_tr);

  u8g2.drawStr(53, 10, "To Do");

  int baseY = 12;

  for (int i = lowerBoundTaskIndex - 3; i <= lowerBoundTaskIndex; i++) {
    JsonVariant task = tasks[i];
    drawTodoTile(task["text"].as<String>(), task["completed"], baseY);

    if (i == selectedTaskIndex) {
      u8g2.drawXBMP(2, baseY + 3, 3, 5, arrow_right);
      u8g2.drawXBMP(123, baseY + 3, 3, 5, arrow_left);
    }
    
    baseY += 13;
  }

  u8g2.sendBuffer();
}


void drawTodoTile(String title, bool isCompleted, int baseY) {
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_haxrcorp4089_tr);

  u8g2.setCursor(19, baseY + 9);
  u8g2.print(title);
  u8g2.drawLine(16, baseY, 8, baseY);
  u8g2.drawLine(7, baseY + 9, 7, baseY + 1);
  u8g2.drawLine(8, baseY + 10, 16, baseY + 10);
  u8g2.drawLine(17, baseY + 1, 17, baseY + 9);
  u8g2.drawLine(18, baseY + 10, 119, baseY + 10);
  u8g2.drawLine(18, baseY, 119, baseY);
  u8g2.drawLine(120, baseY + 1, 120, baseY + 9);
  if (isCompleted) {
    u8g2.drawXBMP(10, baseY + 3, 5, 5, check);
  }
}

void configureServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/style.css", "text/css");
  });
  server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/app.js", "text/javascript");
  });

  server.on("/tasks", HTTP_GET, [](AsyncWebServerRequest *request) {
    String content;
    File file = LittleFS.open("/tasks.json", "r");
    if (!file) content = "[]";
    content = file.readString();
    file.close();
    request->send(200, "application/json", content);
  });

  AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler("/tasks", [](AsyncWebServerRequest *request, JsonVariant &json) {
    File file = LittleFS.open("/tasks.json", "w");
    if (!file) {
      request->send(500, "text/plain", "Could not open file");
      return;
    }
    serializeJson(json, file);
    file.close();
    request->send(200, "text/plain", "Saved");
  });

  server.addHandler(handler);
}

bool saveTasks() {
  File file = LittleFS.open("/tasks.json", "w");
  if (!file) return false;
  serializeJson(doc, file);
  file.close();
  return true;
}

void loadTasks() {
  String content;
  File file = LittleFS.open("/tasks.json", "r");
  if (!file) content = "[]";
  content = file.readString();
  deserializeJson(doc, content);
  file.close();
  tasks = doc.as<JsonArray>();
}

void changeServerState() {
  u8g2.clearBuffer();

  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_haxrcorp4089_tr);

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
    }

    u8g2.drawXBMP(55, 7, 19, 16, image_wifi_bits);
    u8g2.drawStr(42, 36, "Wi-Fi is on");
    u8g2.setCursor(37, 51);
    u8g2.print(WiFi.localIP());
    u8g2.sendBuffer();

    configureServer();
    server.begin();
    delay(2000);
  } else {
    u8g2.drawXBMP(55, 7, 19, 16, image_wifi_not_connected_bits);
    u8g2.drawStr(42, 36, "Wi-Fi is off");
    u8g2.sendBuffer();

    server.end();
    WiFi.disconnect(true);
    loadTasks();

    delay(2000);
    lastUpdate = millis();
    drawTodos();
  }
}
