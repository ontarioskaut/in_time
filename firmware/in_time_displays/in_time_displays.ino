#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

// === CONFIGURATION ===
#define WIFI_SSID      "in_time"
#define WIFI_PASSWORD  "intime2025"
#define SERVER_HOST    "192.168.50.1"
#define SERVER_PORT    8080
#define SERVER_PATH    "/display_data"

#define SERIAL_BAUD    115200
#define DEFAULT_RENEW_MS  10000

#define MAX_TIMES          35
#define MAX_ANNOUNCEMENTS   5

struct TimeEntry {
  String label;
  int value;
};

struct AnnouncementEntry {
  String line1;
  String line2;
  int duration;
};

// === GLOBAL VARIABLES ===
WiFiClient client;

TimeEntry times[MAX_TIMES];
int numTimes = 0;

AnnouncementEntry announcements[MAX_ANNOUNCEMENTS];
int numAnnouncements = 0;

int screenDelay = 4000;
unsigned long renewInterval = DEFAULT_RENEW_MS;
unsigned long lastFetch = 0;

void connectWiFiLoopUntilSuccess() {
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(F("[WiFi] Connecting to "));
    Serial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println(F("[WiFi] Connection failed. Retrying in 2s..."));
      delay(2000);
    }
  }

  Serial.println(F("[WiFi] Connected."));
  Serial.print(F("[WiFi] IP address: "));
  Serial.println(WiFi.localIP());
}

bool getJSON(String& json) {
  json = "";

  if (!client.connect(SERVER_HOST, SERVER_PORT)) {
    Serial.println(F("[ERROR] Connection to server failed."));
    return false;
  }

  Serial.println(F("[HTTP] Sending request..."));
  client.print(String("GET ") + SERVER_PATH + " HTTP/1.0\r\nHost: " + SERVER_HOST + "\r\nConnection: close\r\n\r\n");

  // Read response
  bool headersEnded = false;
  json = "";
  String line;

  unsigned long lastRead = millis();

  while (client.connected() || client.available()) {
    while (client.available()) {
      char c = client.read();
      lastRead = millis();

      if (!headersEnded) {
        if (c == '\r') continue;  // ignore
        if (c == '\n') {
          if (line.length() == 0) {
            headersEnded = true;
            Serial.println(F("[DEBUG] End of headers."));
          }
          line = "";
        } else {
          line += c;
        }
      } else {
        json += c;
      }
    }

    // Timeout if no data received for 3s
    if (millis() - lastRead > 3000) {
      Serial.println(F("[WARN] Timeout waiting for data."));
      break;
    }
  }

  client.stop();

  if (json.isEmpty()) {
    Serial.println(F("[WARN] No JSON received."));
    return false;
  }

  Serial.print(F("[DEBUG] Raw JSON: "));
  Serial.println(json);
  return true;
}

void parseAndPrintJSON(const String& json) {
  numTimes = 0;
  numAnnouncements = 0;

  StaticJsonDocument<4096> doc;
  DeserializationError err = deserializeJson(doc, json);

  if (err) {
    Serial.print(F("[ERROR] JSON parse failed: "));
    Serial.println(err.c_str());
    return;
  }

  // renew_interval
  if (doc.containsKey("renew_interval")) {
    renewInterval = doc["renew_interval"];
    Serial.print(F("renew_interval: "));
    Serial.println(renewInterval);
  }

  // screen_delay
  if (doc.containsKey("screen_delay")) {
    screenDelay = doc["screen_delay"];
    Serial.print(F("screen_delay: "));
    Serial.println(screenDelay);
  }

  // times
  JsonObject timesObj = doc["times"];
  for (JsonPair kv : timesObj) {
    if (numTimes < MAX_TIMES) {
      times[numTimes].label = kv.key().c_str();
      times[numTimes].value = kv.value().as<int>();
      Serial.print(F("Time: "));
      Serial.print(times[numTimes].label);
      Serial.print(F(" = "));
      Serial.println(times[numTimes].value);
      numTimes++;
    }
  }

  // announcements
  JsonArray annArray = doc["announcements"];
  for (JsonArray arr : annArray) {
    if (numAnnouncements < MAX_ANNOUNCEMENTS) {
      announcements[numAnnouncements].line1 = arr[0].as<String>();
      announcements[numAnnouncements].line2 = arr[1].as<String>();
      announcements[numAnnouncements].duration = screenDelay;

      Serial.print(F("Announcement: "));
      Serial.print(announcements[numAnnouncements].line1);
      Serial.print(F(" | "));
      Serial.print(announcements[numAnnouncements].line2);
      Serial.print(F(" | duration="));
      Serial.println(screenDelay);

      numAnnouncements++;
    }
  }
}

void simulateScreens() {
  Serial.println(F("[Display] Simulating screens"));

  // Times
  if (numTimes > 0) {
    int timesPerScreen = 4;
    int numScreens = (numTimes + timesPerScreen - 1) / timesPerScreen;

    for (int s = 0; s < numScreens; s++) {
      Serial.print(F("[Screen "));
      Serial.print(s + 1);
      Serial.println(F("]"));

      for (int i = 0; i < timesPerScreen; i += 2) {
        int idx1 = s * timesPerScreen + i;
        int idx2 = idx1 + 1;

        String line;
        if (idx1 < numTimes) {
          line += times[idx1].label + ": " + String(times[idx1].value);
        }
        if (idx2 < numTimes) {
          line += "   " + times[idx2].label + ": " + String(times[idx2].value);
        }
        Serial.println(line);
      }

      Serial.print(F("[Delay "));
      Serial.print(screenDelay);
      Serial.println(F(" ms]"));
    }
  }

  // Announcements
  if (numAnnouncements > 0) {
    for (int i = 0; i < numAnnouncements; i++) {
      Serial.print(F("[Announcement Screen "));
      Serial.print(i + 1);
      Serial.println(F("]"));

      Serial.println(announcements[i].line1);
      Serial.println(announcements[i].line2);

      Serial.print(F("[Delay "));
      Serial.print(announcements[i].duration);
      Serial.println(F(" ms]"));
    }
  }
}

void safeFetch() {
  while (true) {
    String json;
    if (getJSON(json)) {
      parseAndPrintJSON(json);
      simulateScreens();
      return;
    } else {
      Serial.println(F("[Fetch] Fetch failed. Reconnecting WiFi..."));
      connectWiFiLoopUntilSuccess();
      delay(500);
    }
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(100);
  connectWiFiLoopUntilSuccess();
}

void loop() {
  if (millis() - lastFetch >= renewInterval) {
    Serial.println(F("[Loop] Starting fetch cycle."));
    safeFetch();
    lastFetch = millis();
  }
}
