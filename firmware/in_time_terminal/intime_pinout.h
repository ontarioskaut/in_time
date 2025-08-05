#ifndef intime_pinout_h
#define intime_pinout_h

// SPI
#define SCK_PIN 26
#define MISO_PIN 14
#define MOSI_PIN 12

// I2C
#define SDA_PIN 21
#define SCL_PIN 22

// I2C adresses
#define OLED_ADDR 0x3C // SSD1306
#define BAT_ADDR 0x36 // MAX17048G

// RFID (over SPI)
#define RFID_CS_PIN 32
#define RFID_RST_PIN 27

// RA-01_SH (over SPI + controll)
#define RA01_CS_PIN 34
#define RA01_RST_PIN 35
#define RA01_DIO0_PIN 33

// GPS TU10-F
#define GPS_TX 16
#define GPS_RX 17

// Rotary encoder
#define ENC_A_PIN 18
#define ENC_B_PIN 19
#define ENC_S_PIN 25

// Outputs
#define BUZZER_PIN 15
#define LED_PIN 13

#endif