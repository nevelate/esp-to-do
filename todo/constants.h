#pragma once

#define SLEEP_DELAY 10000
#define TO_DO_OFFSET 15

// Pins
#define I2C_SDA D4 
#define I2C_SCL D5

#define ENC_A D1
#define ENC_B D2
#define ENC_SWITCH D3

#define BUTTON_PIN_BITMASK 0x200000000 // 2^33 in hex

// Your WiFi SSID and password
const char* ssid = "ssid";
const char* password = "password";