#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Flippity210.h"

#include <SoftwareSerial.h>

#define DISP_W_UNIT 28
#define DISP_H 19

// === Serial input selection ===
#define USE_USB_SERIAL  // comment out to use SoftwareSerial

#if !defined(USE_USB_SERIAL)
#define SW_RX_PIN 10
#define SW_TX_PIN 11
SoftwareSerial swSerial(SW_RX_PIN, SW_TX_PIN);
#endif

Stream* cmdSerial = nullptr;

// === Display ===
int displays = 5;
Flippity210 display(displays, 4, 5);

#define MAX_LINE 500
char lineBuffer[MAX_LINE];
uint16_t lineLen = 0;

static uint8_t b64Val(char c) {
  if (c >= 'A' && c <= 'Z') return (uint8_t)(c - 'A');
  if (c >= 'a' && c <= 'z') return (uint8_t)(c - 'a' + 26);
  if (c >= '0' && c <= '9') return (uint8_t)(c - '0' + 52);
  if (c == '+') return 62;
  if (c == '/') return 63;
  return 0;
}

// Decodes one 4-char Base64 quantum into up to 3 bytes.
// in must be 4 chars; out must have room for 3.
// Returns number of output bytes (1..3; 0 on error).
static uint8_t decodeB64Quantum(const char *in, uint8_t *out) {
  // Handle padding '=' cases:
  bool pad2 = (in[2] == '=');
  bool pad3 = (in[3] == '=');

  uint32_t v =
      ((uint32_t)b64Val(in[0]) << 18) |
      ((uint32_t)b64Val(in[1]) << 12) |
      ((uint32_t)b64Val(pad2 ? 'A' : in[2]) << 6) |
      ((uint32_t)b64Val(pad3 ? 'A' : in[3]));

  out[0] = (v >> 16) & 0xFF;
  if (pad2) return 1;

  out[1] = (v >> 8) & 0xFF;
  if (pad3) return 2;

  out[2] = v & 0xFF;
  return 3;
}

// Parse N hex chars (1-8 typical) into uint32_t. Stops at first non-hex.
static uint32_t parseHexN(const char *p, uint8_t maxChars, const char **endOut) {
  uint32_t v = 0;
  uint8_t n = 0;
  while (n < maxChars) {
    char c = p[n];
    uint8_t d;
    if (c >= '0' && c <= '9') d = (uint8_t)(c - '0');
    else if (c >= 'A' && c <= 'F') d = (uint8_t)(c - 'A' + 10);
    else if (c >= 'a' && c <= 'f') d = (uint8_t)(c - 'a' + 10);
    else break;
    v = (v << 4) | d;
    n++;
  }
  if (endOut) *endOut = p + n;
  return v;
}

// Reverse bit order: MSB <-> LSB
static uint8_t reverseBits(uint8_t b) {
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
}

bool processFrame(const char *line) {
  Serial.print(F("Processing: "));
  Serial.println(line);

  // Must start with !FRAME;
  const char prefix[] = "!FRAME;";
  const size_t prefixLen = sizeof(prefix) - 1;
  if (strncmp(line, prefix, prefixLen) != 0) {
    Serial.println(F("No FRAME marker, ignoring."));
    return false;
  }
  const char *p = line + prefixLen;

  // --- Optional ADDR=xx; ---
  if (strncmp(p, "ADDR=", 5) == 0) {
    const char *addrStart = p + 5;
    const char *addrEnd;
    uint32_t addr = parseHexN(addrStart, 2, &addrEnd);
    if (*addrEnd != ';') {
      Serial.println(F("ADDR parse error."));
      return false;
    }
    display.setAddress((uint8_t)addr);
    Serial.print(F("I2C addr set: 0x"));
    Serial.println((uint8_t)addr, HEX);
    p = addrEnd + 1;
  }

  // --- Command ---
  if (!*p) { Serial.println(F("Missing command.")); return false; }
  const char *cmdEnd = strchr(p, ';');
  if (!cmdEnd) { Serial.println(F("Missing command separator.")); return false; }
  uint32_t cmd = parseHexN(p, (uint8_t)(cmdEnd - p), nullptr);
  Serial.print(F("Cmd: 0x"));
  Serial.println((uint8_t)cmd, HEX);
  p = cmdEnd + 1;

  // --- Length ---
  if (!*p) { Serial.println(F("Missing length.")); return false; }
  const char *lenEnd = strchr(p, ';');
  if (!lenEnd) { Serial.println(F("Missing length separator.")); return false; }
  uint32_t declaredLen = parseHexN(p, (uint8_t)(lenEnd - p), nullptr);
  Serial.print(F("Declared length: "));
  Serial.println((uint16_t)declaredLen);
  p = lenEnd + 1;

  // --- Buffer setup ---
  uint8_t *buf = display.getBuffer();
  if (!buf) {
    Serial.println(F("ERROR: display buffer null!"));
    return false;
  }

// Calculate expected buffer size: round up to nearest byte
const uint16_t bufCap = DISP_H * (((DISP_W_UNIT * displays) + 7) / 8);

if (declaredLen != bufCap) {
  Serial.print(F("Length mismatch (expected "));
  Serial.print(bufCap);
  Serial.print(F(" got "));
  Serial.print(declaredLen);
  Serial.println(F(")"));
}

  // --- Base64 decode ---
  uint16_t outIndex = 0;
  while (p[0] && p[1]) {
    if (!p[2] || !p[3]) break;
    if (p[0] <= ' ' || p[1] <= ' ' || p[2] <= ' ' || p[3] <= ' ') break;

    uint8_t tmp[3];
    uint8_t nOut = decodeB64Quantum(p, tmp);

for (uint8_t i = 0; i < nOut; i++) {
    if (outIndex >= declaredLen) break;
    if (outIndex >= bufCap) break;
    buf[outIndex++] = tmp[i];
}
    

    if (p[2] == '=' || p[3] == '=') {
      p += 4;
      break;
    }
    p += 4;
    if (outIndex >= declaredLen || outIndex >= bufCap) break;
  }

  Serial.print(F("Decoded bytes: "));
  Serial.println(outIndex);

  if (outIndex < declaredLen) {
    Serial.println(F("WARN: decoded fewer bytes than declared length."));
  } else if (outIndex > declaredLen) {
    Serial.println(F("NOTE: decoded more than declared? (clamped)"));
  }
  if (outIndex > bufCap) {
    Serial.println(F("ERROR: payload larger than display buffer; truncated."));
  }

  // --- Execute command ---
  switch ((uint8_t)cmd) {
    case 0x00:
      display.update();
      Serial.println(F("display.update()"));
      break;
    case 0x01:
      display.updateDots();
      Serial.println(F("display.updateDots()"));
      break;
    case 0x02:
      display.updateLeds();
      Serial.println(F("display.updateLeds()"));
      break;
    default:
      Serial.println(F("Unknown cmd -> buffer loaded, no auto update."));
      break;
  }

  Serial.println(F("Frame processed."));
  return true;
}

void setup() {
#if defined(USE_USB_SERIAL)
  Serial.begin(115200);
  cmdSerial = &Serial;
#else
  swSerial.begin(19200);
  cmdSerial = &swSerial;
#endif

  delay(200);
  Serial.println("");
  Serial.println("Starting up");

  display.begin();
  Serial.println("Started");

  display.setRotation(180);
  display.setTextColor(FLIPPITY210_WHITE);
  display.setAnimation(FLIPPITY210_ANIM_SLIDE_RIGHT);
  display.setAnimationSpeed(FLIPPITY210_SPEED_HIGH);
  Serial.println("Animations set");

  display.clear();
  display.update();
  Serial.println("Display initialized");
}

void loop() {
  while (cmdSerial->available()) {
    char c = cmdSerial->read();
    if (c == '\n') {
      lineBuffer[lineLen] = 0;
      if (lineLen > 0) {
        Serial.println("Frame received.");
        processFrame(lineBuffer);
      }
      lineLen = 0;
    } else if (lineLen < MAX_LINE - 1) {
      lineBuffer[lineLen++] = c;
    }
  }
}
