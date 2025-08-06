#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <MFRC522.h>
#include <Adafruit_MAX1704X.h>
#include "intime_pinout.h"

// -------------------- CONFIG --------------------
#define WIFI_SSID "in_time"
#define WIFI_PASSWORD "intime2025"
#define SERVER_HOST "192.168.50.1:5000"

// -------------------- UI --------------------
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, SCL_PIN, SDA_PIN);

// -------------------- Battery --------------------
Adafruit_MAX17048 max17048;
bool batteryAvailable = false;

// -------------------- RFID --------------------
MFRC522 mfrc522(RFID_CS_PIN, RFID_RST_PIN);

// -------------------- Encoder --------------------
volatile int8_t encoderDelta = 0;
volatile uint8_t lastEncoded = 0;
volatile bool encoderClicked = false;

void IRAM_ATTR onEncoderClick() {
  encoderClicked = true;
}
void IRAM_ATTR handleEncoder() {
  uint8_t MSB = digitalRead(ENC_A_PIN);
  uint8_t LSB = digitalRead(ENC_B_PIN);
  uint8_t encoded = (MSB << 1) | LSB;
  uint8_t sum = (lastEncoded << 2) | encoded;
  if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderDelta++;
  if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderDelta--;
  lastEncoded = encoded;
}

// -------------------- States --------------------
enum AppState {
  STATE_WIFI_CONNECT,
  STATE_MAIN_MENU,
  STATE_USER_INFO,
  STATE_PAY_TERMINAL,
  STATE_LOAD_COINS
};

AppState currentState = STATE_WIFI_CONNECT;

// -------------------- Menu --------------------
int menuIndex = 0;
const char* menuItems[] = { "User/Coin Info", "Pay Terminal", "Load Coins" };
const int numMenuItems = 3;

// -------------------- Long press --------------------
unsigned long buttonPressTime = 0;
bool buttonHeld = false;
const unsigned long LONG_PRESS_MS = 800;

// -------------------- Helpers --------------------
String formatUid(MFRC522::Uid uid) {
  char buf[32] = "";
  for (byte i = 0; i < uid.size; i++) {
    char hexByte[4];
    sprintf(hexByte, "%02X", uid.uidByte[i]);
    strcat(buf, hexByte);
    if (i < uid.size - 1) strcat(buf, ":");
  }
  return String(buf);
}

String formatTime(long seconds) {
  long s = abs(seconds);
  long days = s / 86400;
  long hours = (s % 86400) / 3600;
  long minutes = (s % 3600) / 60;
  long secs = s % 60;
  char buf[24];
  if (seconds < 0) {
    sprintf(buf, "-%02ld:%02ld:%02ld:%02ld", days, hours, minutes, secs);
  } else {
    sprintf(buf, "%02ld:%02ld:%02ld:%02ld", days, hours, minutes, secs);
  }
  return String(buf);
}

String normalizeCzech(const String &s) {
  String out;
  out.reserve(s.length());

  for (int i = 0; i < s.length(); ) {
    uint8_t c = s[i];

    // Single byte (ASCII)
    if (c < 0x80) {
      out += (char)c;
      i++;
    }
    // Two-byte UTF-8
    else if ((c & 0xE0) == 0xC0 && i + 1 < s.length()) {
      uint8_t c2 = s[i + 1];
      uint16_t codepoint = ((c & 0x1F) << 6) | (c2 & 0x3F);

      char mapped = '?';

      switch (codepoint) {
        case 0xE1: mapped = 'a'; break;            // á
        case 0xC1: mapped = 'A'; break;            // Á
        case 0x10D: mapped = 'c'; break;           // č
        case 0x10C: mapped = 'C'; break;           // Č
        case 0x10F: mapped = 'd'; break;           // ď
        case 0x10E: mapped = 'D'; break;           // Ď
        case 0xE9: mapped = 'e'; break;            // é
        case 0xC9: mapped = 'E'; break;            // É
        case 0x11B: mapped = 'e'; break;           // ě
        case 0x11A: mapped = 'E'; break;           // Ě
        case 0xED: mapped = 'i'; break;            // í
        case 0xCD: mapped = 'I'; break;            // Í
        case 0x148: mapped = 'n'; break;           // ň
        case 0x147: mapped = 'N'; break;           // Ň
        case 0xF3: mapped = 'o'; break;            // ó
        case 0xD3: mapped = 'O'; break;            // Ó
        case 0x159: mapped = 'r'; break;           // ř
        case 0x158: mapped = 'R'; break;           // Ř
        case 0x161: mapped = 's'; break;           // š
        case 0x160: mapped = 'S'; break;           // Š
        case 0x165: mapped = 't'; break;           // ť
        case 0x164: mapped = 'T'; break;           // Ť
        case 0xFA: mapped = 'u'; break;            // ú
        case 0xDA: mapped = 'U'; break;            // Ú
        case 0x16F: mapped = 'u'; break;           // ů
        case 0x16E: mapped = 'U'; break;           // Ů
        case 0xFD: mapped = 'y'; break;            // ý
        case 0xDD: mapped = 'Y'; break;            // Ý
        case 0x17E: mapped = 'z'; break;           // ž
        case 0x17D: mapped = 'Z'; break;           // Ž
      }

      if (mapped != '?') out += mapped;
      i += 2;
    }
    else {
      i++;
    }
  }

  return out;
}

void beepSuccess() {
  tone(BUZZER_PIN, 2000, 100);
}

void beepError() {
  tone(BUZZER_PIN, 400, 400);
}

void flashScreen() {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 64);
  u8g2.sendBuffer();
  delay(150);
  u8g2.clearBuffer();
  u8g2.sendBuffer();
  delay(100);
}

void checkButtonLongPress() {
  if (digitalRead(ENC_S_PIN) == LOW) {
    if (!buttonHeld) {
      buttonPressTime = millis();
      buttonHeld = true;
    } else if (millis() - buttonPressTime > LONG_PRESS_MS) {
      flashScreen();
      currentState = STATE_MAIN_MENU;
      encoderClicked = false;  // prevent short click after long press
      buttonHeld = false;
    }
  } else {
    buttonHeld = false;
  }
}

// -------------------- Networking --------------------
String apiGet(String endpoint, String params) {
  HTTPClient http;
  String url = "http://" + String(SERVER_HOST) + endpoint + "?" + params;
  http.begin(url);
  int httpCode = http.GET();
  String payload = "";
  if (httpCode > 0) payload = http.getString();
  http.end();
  return payload;
}

String searchTag(String tagUid) {
  return apiGet("/api/nodes/search_tags", "tag_id=" + tagUid);
}

String subtractTime(String tagUid, long seconds) {
  return apiGet("/api/nodes/subtract_time", "time_to_subtract=" + String(seconds) + "&user_tag_id=" + tagUid);
}

String addCoin(String coinUid, String userUid) {
  return apiGet("/api/nodes/add_coinval", "coin_tag_id=" + coinUid + "&user_tag_id=" + userUid);
}

// -------------------- UI: WiFi icon --------------------
void drawWiFiIcon(int x, int y) {
  int bars = 0;
  if (WiFi.isConnected()) {
    int32_t rssi = WiFi.RSSI();
    if (rssi > -60) bars = 3;
    else if (rssi > -70) bars = 2;
    else if (rssi > -80) bars = 1;
    else bars = 0;
  }
  for (int i = 0; i < 3; i++) {
    if (i < bars) u8g2.drawBox(x + i * 4, y - i * 2, 3, i * 2 + 2);
    else u8g2.drawFrame(x + i * 4, y - i * 2, 3, i * 2 + 2);
  }
}

// -------------------- UI: Top bar --------------------
void drawTopBar() {
  u8g2.setFont(u8g2_font_5x7_tr);
  char batStr[20];
  if (batteryAvailable) {
    float vcell = max17048.cellVoltage();
    int soc = (int)max17048.cellPercent();
    sprintf(batStr, "%.2fV %d%%", vcell, soc);
  } else {
    sprintf(batStr, "NoBat");
  }
  u8g2.drawStr(0, 7, batStr);

  drawWiFiIcon(110, 7);
  u8g2.drawHLine(0, 9, 128);
}

void drawMenu() {
  u8g2.setFont(u8g2_font_6x12_tr);
  for (int i = 0; i < numMenuItems; i++) {
    if (i == menuIndex) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, 12 + i * 16, 128, 16);
      u8g2.setDrawColor(0);
      u8g2.drawStr(2, 24 + i * 16, menuItems[i]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(2, 24 + i * 16, menuItems[i]);
    }
  }
}

void drawCenteredBoxMessage(const char* msg) {
  u8g2.setFont(u8g2_font_fub14_tr);
  int16_t textWidth = u8g2.getStrWidth(msg);
  int16_t boxWidth = textWidth + 12;
  int16_t boxHeight = 28;

  int16_t x = (128 - boxWidth) / 2;
  int16_t y = (64 - boxHeight) / 2;

  u8g2.setDrawColor(1);
  u8g2.drawRBox(x, y+9, boxWidth, boxHeight, 6);

  u8g2.setDrawColor(0);
  u8g2.setCursor((128 - textWidth) / 2, (64 + 14) / 2 + 9);
  u8g2.print(msg);

  u8g2.setDrawColor(1);
}

// -------------------- RFID handler --------------------
String waitForCard() {
  while (true) {
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      String uid = formatUid(mfrc522.uid);
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
      beepSuccess();
      return uid;
    }
    checkButtonLongPress();
    if (currentState == STATE_MAIN_MENU) return "";
    delay(50);
  }
}

// -------------------- Modes --------------------

// --- User/Coin Info ---
void enterUserInfo() {
  while (true) {
    u8g2.clearBuffer();
    drawTopBar();
    drawCenteredBoxMessage("Scan tag");
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.sendBuffer();

    String uid = waitForCard();
    if (currentState == STATE_MAIN_MENU) return;

    String response = searchTag(uid);

    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, response);
    if (err) continue;

    if (doc.containsKey("error")) {
      beepError();
      u8g2.clearBuffer();
      drawTopBar();
      u8g2.drawStr(0, 20, "No match");
      u8g2.sendBuffer();
      delay(2000);
      continue;
    }

    String type = doc["type"].as<String>();
    u8g2.clearBuffer();
    drawTopBar();

    if (type == "user") {
      String name = normalizeCzech(doc["user_name"].as<String>());
      String acro = normalizeCzech(doc["user_acro"].as<String>());
      long rem = doc["remaining_time"].as<long long>();

      u8g2.setFont(u8g2_font_6x12_tr);
      u8g2.drawStr(0, 20, ("User: " + name).c_str());
      u8g2.drawStr(0, 31, ("Acro: " + acro).c_str());
      u8g2.drawStr(0, 42, ("Seconds: " + String(rem)).c_str());

      u8g2.setFont(u8g2_font_fub14_tr);
      u8g2.drawStr(0, 60, formatTime(rem).c_str());
    } else {
      long val = doc["coin_value"].as<long long>();
      String cat = doc["coin_category_name"].as<String>();
      u8g2.setFont(u8g2_font_6x12_tr);
      u8g2.drawStr(0, 20, ("Coin: " + cat).c_str());
      u8g2.drawStr(0, 31, ("Val: " + String(val)).c_str());
      u8g2.drawStr(0, 42, ("Time: " + formatTime(val)).c_str());
      u8g2.drawStr(0, 53, doc["active"] == 0 ? "Inactive" : "Active");
    }
    u8g2.sendBuffer();
    unsigned long start = millis();
    while (millis() - start < 5000) {
      checkButtonLongPress();
      if (currentState == STATE_MAIN_MENU) return;
      delay(20);
    }
  }
}

// --- Pay Terminal ---
void enterPayTerminal() {
  int fields[4] = { 0, 0, 0, 0 };  // days, hours, minutes, seconds
  int fieldIndex = 3;              // start editing seconds

  // Editing phase
  while (true) {
    u8g2.clearBuffer();
    drawTopBar();
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(0, 18, "Set time (D:H:M:S)");

    // Show fields big
    u8g2.setFont(u8g2_font_fub17_tr);
    char buf[20];
    sprintf(buf, "%02d:%02d:%02d:%02d", fields[0], fields[1], fields[2], fields[3]);
    u8g2.drawStr(0, 60, buf);

    // Indicate current field
    int xPos[] = { 0, 32, 64, 96 };
    u8g2.drawHLine(xPos[fieldIndex], 63, 28);

    u8g2.sendBuffer();

    if (encoderDelta != 0) {
      fields[fieldIndex] += encoderDelta;
      if (fieldIndex == 1) fields[1] = constrain(fields[1], 0, 23);
      else if (fieldIndex == 2 || fieldIndex == 3) fields[fieldIndex] = constrain(fields[fieldIndex], 0, 59);
      else if (fields[0] < 0) fields[0] = 0;
      encoderDelta = 0;
    }

    if (encoderClicked) {
      encoderClicked = false;
      fieldIndex--;
      if (fieldIndex < 0) {
        break;  // Done editing
      }
    }

    checkButtonLongPress();
    if (currentState == STATE_MAIN_MENU) return;
    delay(20);
  }

  // Confirm step
  long totalSeconds = fields[0] * 86400L + fields[1] * 3600L + fields[2] * 60L + fields[3];
  while (true) {
    u8g2.clearBuffer();
    drawTopBar();
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(0, 20, "Press to confirm");
    u8g2.drawStr(0, 42, "Subtracted amount:");
    u8g2.setFont(u8g2_font_fub14_tr);
    u8g2.drawStr(0, 60, formatTime(totalSeconds).c_str());
    u8g2.sendBuffer();

    if (encoderClicked) {
      encoderClicked = false;
      // Enter multiple scan mode
      while (true) {
        u8g2.clearBuffer();
        drawTopBar();
        u8g2.setFont(u8g2_font_6x12_tr);
        u8g2.drawStr(0, 20, "Scan user tag...");
        u8g2.drawStr(0, 42, "Paying:");
        u8g2.setFont(u8g2_font_fub14_tr);
        u8g2.drawStr(0, 60, formatTime(totalSeconds).c_str());
        u8g2.setFont(u8g2_font_6x12_tr);
        u8g2.sendBuffer();

        String uid = waitForCard();
        if (currentState == STATE_MAIN_MENU) return;

        String res = subtractTime(uid, totalSeconds);
        StaticJsonDocument<256> doc;
        deserializeJson(doc, res);

        u8g2.clearBuffer();
        drawTopBar();
        if (doc.containsKey("error")) {
          beepError();
          u8g2.drawStr(0, 20, "Error user");
        } else {
          beepSuccess();
          long newTime = doc["user_time"].as<long long>();
          u8g2.setFont(u8g2_font_6x12_tr);
          u8g2.drawStr(0, 20, "Success!");
          u8g2.drawStr(0, 42, "New user time:");
          u8g2.setFont(u8g2_font_fub14_tr);
          u8g2.drawStr(0, 60, formatTime(newTime).c_str());
          u8g2.setFont(u8g2_font_6x12_tr);
        }
        u8g2.sendBuffer();
        delay(2000);
      }
    }

    checkButtonLongPress();
    if (currentState == STATE_MAIN_MENU) return;
    delay(20);
  }
}

// --- Load Coins ---
void enterLoadCoins() {
  // Step 1: Scan user
  u8g2.clearBuffer();
  drawTopBar();
  drawCenteredBoxMessage("Scan user");
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.sendBuffer();
  String userUid = waitForCard();
  if (currentState == STATE_MAIN_MENU) return;

  // Step 2: Continuous coin scanning
  while (true) {
    u8g2.clearBuffer();
    drawTopBar();
    drawCenteredBoxMessage("Scan coin");
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.sendBuffer();

    String coinUid = waitForCard();
    if (currentState == STATE_MAIN_MENU) return;

    String res = addCoin(coinUid, userUid);
    StaticJsonDocument<256> doc;
    deserializeJson(doc, res);

    u8g2.clearBuffer();
    drawTopBar();
    if (doc.containsKey("error")) {
      beepError();
      u8g2.setFont(u8g2_font_6x12_tr);
      u8g2.drawStr(0, 20, "Error coin");
    } else {
      beepSuccess();
      long newTime = doc["user_time"].as<long long>();
      long coinVal = doc["coin_value"].as<long long>();
      u8g2.setFont(u8g2_font_6x12_tr);
      u8g2.drawStr(0, 20, ("Seconds: " + String(coinVal)).c_str());
      u8g2.drawStr(0, 31, ("Earned: " + formatTime(coinVal)).c_str());
      u8g2.drawStr(0, 42, "New time:");
      u8g2.setFont(u8g2_font_fub14_tr);
      u8g2.drawStr(0, 60, formatTime(newTime).c_str());
    }
    u8g2.sendBuffer();

    unsigned long start = millis();
    while (millis() - start < 4000) {
      checkButtonLongPress();
      if (currentState == STATE_MAIN_MENU) return;
      delay(20);
    }
  }
}

// -------------------- Setup --------------------
void setup() {
  Serial.begin(115200);
  // Encoder pins
  pinMode(ENC_A_PIN, INPUT_PULLUP);
  pinMode(ENC_B_PIN, INPUT_PULLUP);
  pinMode(ENC_S_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_A_PIN), handleEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B_PIN), handleEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_S_PIN), onEncoderClick, FALLING);

  // Display
  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();

  // RFID
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, RFID_CS_PIN);
  mfrc522.PCD_Init();

  // Battery
  if (max17048.begin(&Wire)) batteryAvailable = true;

  // WiFi connect
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(200);
  delay(1000);

  currentState = STATE_MAIN_MENU;
}

// -------------------- Loop --------------------
void loop() {
  u8g2.clearBuffer();
  drawTopBar();

  switch (currentState) {
    case STATE_MAIN_MENU:
      if (encoderDelta != 0) {
        menuIndex = constrain(menuIndex + (encoderDelta > 0 ? 1 : -1), 0, numMenuItems - 1);
        encoderDelta = 0;
      }
      drawMenu();
      if (encoderClicked) {
        encoderClicked = false;
        if (menuIndex == 0) currentState = STATE_USER_INFO;
        else if (menuIndex == 1) currentState = STATE_PAY_TERMINAL;
        else if (menuIndex == 2) currentState = STATE_LOAD_COINS;
      }
      break;

    case STATE_USER_INFO:
      enterUserInfo();
      break;

    case STATE_PAY_TERMINAL:
      enterPayTerminal();
      break;

    case STATE_LOAD_COINS:
      enterLoadCoins();
      break;
  }

  u8g2.sendBuffer();
  delay(20);
}
