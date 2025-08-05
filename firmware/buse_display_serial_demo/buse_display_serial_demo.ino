#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Flippity210.h"
#include "Fonts/FreeMonoBold12pt7b.h"

int displays = 5;
Flippity210 display(displays, -1, -1);
const uint8_t i2c_addr = 0x08;
const char b64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void encodeAndSendBase64(const uint8_t* buf, uint16_t len) {
  for (uint16_t i = 0; i < len; i += 3) {
    uint32_t val = ((uint32_t)buf[i] << 16) |
                   ((i + 1 < len ? buf[i + 1] : 0) << 8) |
                   ((i + 2 < len ? buf[i + 2] : 0));

    char out[4];
    out[0] = b64chars[(val >> 18) & 0x3F];
    out[1] = b64chars[(val >> 12) & 0x3F];
    out[2] = (i + 1 < len) ? b64chars[(val >> 6) & 0x3F] : '=';
    out[3] = (i + 2 < len) ? b64chars[val & 0x3F] : '=';

    Serial.write(out, 4);
  }
}

void setup() {
  Serial.begin(19200);
  delay(200);

  display.setAddress(i2c_addr);
  display.begin();

  display.setRotation(180);
  display.setTextColor(FLIPPITY210_WHITE);
}

void loop() {
  display.fill();
  sendFrame(0x01);
  delay(1000);

  display.clear();
  display.setFont(&FreeMonoBold12pt7b);
  display.setCursor(5, 18);
  display.println(F("Hotovo!"));
  sendFrame(0x01);
  delay(2000);
}

void sendFrame(uint8_t cmd) {
  const uint16_t bufSize =
    FLIPPITY210_BYTES_PER_ROW *
    FLIPPITY210_SCREEN_RES_H *
    displays;

  uint8_t* buf = display.getBuffer();

  // --- Header ---
  Serial.print(F("!FRAME;ADDR="));
  if (i2c_addr < 16) Serial.print("0");
  Serial.print(i2c_addr, HEX);
  Serial.print(';');

  if (cmd < 16) Serial.print("0");
  Serial.print(cmd, HEX);
  Serial.print(';');

  Serial.print(bufSize, HEX);
  Serial.print(';');

  char encoded[5]; // always 4 chars + null
  for (uint16_t i = 0; i < bufSize; i += 3) {
    uint8_t a = buf[i];
    uint8_t b = (i + 1 < bufSize) ? buf[i + 1] : 0;
    uint8_t c = (i + 2 < bufSize) ? buf[i + 2] : 0;

    encodeAndSendBase64(buf, bufSize);
  }

  Serial.println();
}
