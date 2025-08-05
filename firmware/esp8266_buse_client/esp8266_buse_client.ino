#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Flippity210.h"
#include "Font5x7Fixed.h"
#include "Font4x5Fixed.h"

// =========================
// CONFIGURATION
// =========================
#define WIFI_SSID     "in_time"
#define WIFI_PASSWORD "intime2025"
#define SERVER_HOST   "192.168.50.1:5000"
#define DISPLAY_NAME  "buse5p"
#define POLL_INTERVAL 500
#define NUM_DISPLAYS  5 // number of buse display modules

int displays = 5;
Flippity210 display(displays, 0, 2); // SDA=0, SCL=2 for ESP-01(S)

// =========================
// WIFI MANAGEMENT
// =========================
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());
}

// =========================
// BASE64 DECODE IMPLEMENTATION
// =========================
static const char PROGMEM b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int b64_index(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

bool decodeBase64Frame(const String &b64, std::vector<uint8_t> &out) {
  int len = b64.length();
  out.clear();
  out.reserve((len * 3) / 4);

  int val = 0, valb = -8;
  for (int i = 0; i < len; i++) {
    char c = b64[i];
    if (c == '=' || c == '\n' || c == '\r') continue;
    int idx = b64_index(c);
    if (idx == -1) continue;
    val = (val << 6) + idx;
    valb += 6;
    if (valb >= 0) {
      out.push_back((uint8_t)((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return !out.empty();
}

// =========================
// DRAW FRAME TO DISPLAY
// =========================
void drawFrame(const std::vector<uint8_t> &frame) {
  const int width = display.width();
  const int height = display.height();

  int byteIndex = 0;
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int bitIndex = x % 8;
      if (bitIndex == 0 && x > 0) byteIndex++;

      uint8_t byteVal = frame[byteIndex];
      bool pixelOn = byteVal & (1 << bitIndex);

      if (pixelOn) {
        display.drawPixel(x, y, FLIPPITY210_WHITE);
      } else {
        display.drawPixel(x, y, FLIPPITY210_BLACK);
      }
    }
    if (width % 8 != 0) byteIndex++;
  }

  display.update();
}

// =========================
// FETCH AND DISPLAY LOOP
// =========================
void fetchAndDisplay() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    return;
  }

  WiFiClient client;
  HTTPClient http;

  String url = String("http://") + SERVER_HOST + "/api/display/display_output/" + DISPLAY_NAME + "?format=base64";
  Serial.println("Fetching: " + url);

  if (!http.begin(client, url)) {
    Serial.println("HTTP begin failed");
    return;
  }

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    // Endpoint returns raw base64, no JSON parsing needed
    String b64 = http.getString();

    std::vector<uint8_t> rawFrame;
    if (decodeBase64Frame(b64, rawFrame)) {
      drawFrame(rawFrame);
      Serial.println("Frame updated");
    } else {
      Serial.println("Base64 decode failed");
    }
  } else {
    Serial.printf("HTTP GET failed, code: %d\n", httpCode);
  }

  http.end();
}

// =========================
// SETUP
// =========================
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Booting...");

  connectWiFi();

  display.begin();
  display.setRotation(0);
  display.setTextColor(FLIPPITY210_WHITE);
  display.setAnimationSpeed(FLIPPITY210_SPEED_HIGH);
  display.setAnimation(FLIPPITY210_ANIM_SLIDE_RIGHT);
  display.clear();
  display.update();

  display.fill();
  Serial.println("Display filled");
  display.update();
  delay(1000);
  display.clear();
  display.update();

  Serial.println("Display cleared");

  delay(5000);

  display.clear();
  display.setFont(&Font5x7Fixed);
  display.setCursor(0, 10);
  display.println(F("Displej je"));
  display.setCursor(0, 18);
  display.println(F("READY k pouziti"));
  display.update();
  Serial.println("Display ready text");

  delay(5000);
  Serial.println("Display formatting done");
}

// =========================
// MAIN LOOP
// =========================
void loop() {
  fetchAndDisplay();
  delay(POLL_INTERVAL);
}
