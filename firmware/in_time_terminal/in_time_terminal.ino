#include <SPI.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <MFRC522.h>
#include <Adafruit_MAX1704X.h>
#include "intime_pinout.h"

// === OLED ===
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, SCL_PIN, SDA_PIN);

// === Battery ===
Adafruit_MAX17048 max17048;
bool batteryAvailable = false;

// === RFID ===
MFRC522 mfrc522(RFID_CS_PIN, RFID_RST_PIN);

// === Menu State ===
int menuIndex = 0;
const char* menuItems[] = {"Status", "Settings", "About", "Restart"};
const int numMenuItems = sizeof(menuItems) / sizeof(menuItems[0]);

// Card info
String lastUid = "";
String lastType = "";
unsigned long cardDisplayUntil = 0;

// Encoder variables
volatile int8_t encoderDelta = 0;
volatile uint8_t lastEncoded = 0;

// Button click ISR
volatile bool encoderClicked = false;
void IRAM_ATTR onEncoderClick() {
  encoderClicked = true;
}

// Encoder ISR
void IRAM_ATTR handleEncoder() {
  // Read both channels
  uint8_t MSB = digitalRead(ENC_A_PIN);
  uint8_t LSB = digitalRead(ENC_B_PIN);

  uint8_t encoded = (MSB << 1) | LSB;
  uint8_t sum = (lastEncoded << 2) | encoded;

  // Detect rotation (Gray code sequence)
  if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderDelta++;
  if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderDelta--;

  lastEncoded = encoded;
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Encoder pins
  pinMode(ENC_A_PIN, INPUT_PULLUP);
  pinMode(ENC_B_PIN, INPUT_PULLUP);
  pinMode(ENC_S_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(ENC_A_PIN), handleEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B_PIN), handleEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_S_PIN), onEncoderClick, FALLING);

  // OLED
  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();

  // RFID
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, RFID_CS_PIN);
  mfrc522.PCD_Init();
  byte version = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  if (version == 0x00 || version == 0xFF) {
    Serial.println(F("[ERROR] MFRC522 not responding!"));
    while (1) delay(1000);
  }

  // Battery
  if (max17048.begin(&Wire)) {
    batteryAvailable = true;
  }

  // Splash
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(0, 12, "RFID + Menu Ready");
  u8g2.sendBuffer();
  delay(1000);
}

void loop() {
  unsigned long now = millis();

  // === Battery ===
  float vcell = 0;
  float soc = 0;
  bool validBattery = false;
  if (batteryAvailable) {
    vcell = max17048.cellVoltage();
    soc = max17048.cellPercent();
    if (!isnan(vcell) && !isnan(soc) && vcell > 2.0 && vcell < 5.0)
      validBattery = true;
    else
      batteryAvailable = false;
  }

  // === RFID ===
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    char uidStr[32] = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      char hexByte[4];
      sprintf(hexByte, "%02X", mfrc522.uid.uidByte[i]);
      strcat(uidStr, hexByte);
      if (i < mfrc522.uid.size - 1) strcat(uidStr, " ");
    }
    MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
    lastUid = String(uidStr);
    lastType = mfrc522.PICC_GetTypeName(piccType);
    cardDisplayUntil = now + 3000;

    Serial.print(F("[CARD] UID: "));
    Serial.println(lastUid);
    Serial.print(F("[CARD] Type: "));
    Serial.println(lastType);

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }

  // === Encoder handling ===
  if (encoderDelta != 0) {
    if (encoderDelta > 0 && menuIndex < numMenuItems - 1) {
      menuIndex++;
    } else if (encoderDelta < 0 && menuIndex > 0) {
      menuIndex--;
    }
    encoderDelta = 0;

    Serial.print(F("[MENU] Moved to: "));
    Serial.println(menuItems[menuIndex]);
  }

  if (encoderClicked) {
    encoderClicked = false;
    Serial.print(F("[MENU] Selected: "));
    Serial.println(menuItems[menuIndex]);
    tone(BUZZER_PIN, 440, 200);
  }

  // === Display ===
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_5x7_tr);
  if (validBattery) {
    char batStr[20];
    sprintf(batStr, "Bat: %.2fV  %d%%", vcell, (int)soc);
    u8g2.drawStr(0, 7, batStr);
  } else {
    u8g2.drawStr(0, 7, "Battery N/A");
  }
  u8g2.drawHLine(0, 9, 128);

  if (now < cardDisplayUntil) {
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(0, 20, "Card UID:");
    u8g2.drawStr(0, 32, lastUid.c_str());
    u8g2.drawStr(0, 44, lastType.c_str());
  } else {
    u8g2.setFont(u8g2_font_6x12_tr);
    for (int i = 0; i < numMenuItems; i++) {
      if (i == menuIndex) {
        u8g2.setDrawColor(1);
        u8g2.drawBox(0, 12 + i * 12, 128, 12);
        u8g2.setDrawColor(0);
        u8g2.drawStr(2, 22 + i * 12, menuItems[i]);
        u8g2.setDrawColor(1);
      } else {
        u8g2.drawStr(2, 22 + i * 12, menuItems[i]);
      }
    }
  }

  u8g2.sendBuffer();
  delay(5);
}
