/*
 * Beer Race — ESP32-C3
 *
 * Библиотеки (Arduino IDE → Sketch → Include Library → Manage Libraries):
 *   - ESPAsyncWebServer by lacamera (или me-no-dev)
 *   - AsyncTCP by lacamera (для ESP32)
 *   - ArduinoJson by Benoit Blanchon (v6)
 *   - HX711 by Bogdan Necula
 *
 * Плата: ESP32C3 Dev Module
 * Upload Speed: 921600 (или 115200 если не грузится)
 * USB CDC On Boot: Enabled (часто нужно для Serial на C3)
 *
 * IP адрес: откройте Serial Monitor 115200 после прошивки — строка "IP address:"
 */

#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <HX711.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

// ============ Настройки Wi-Fi (замените на свои) ============
const char* WIFI_SSID     = "4G-CPE_2162";
const char* WIFI_PASSWORD = "1234567890";

// mDNS: http://beerrace.local (работает не на всех телефонах)
const char* MDNS_HOSTNAME = "beerrace";

// ============ Пины HX711 (ESP32-C3) ============
#define DT_PIN  4
#define SCK_PIN 5

#define CALIBRATION_FACTOR  33.7f
#define DRINK_THRESHOLD     60.0f   // г — старт таймера
#define FINISH_THRESHOLD    20.0f   // г — допуск до baseline
#define AVERAGE_SAMPLES     10

#define LEADERBOARD_FILE "/leaderboard.json"
#define MAX_PLAYERS      50

enum GameState : uint8_t {
  IDLE = 0,
  WAITING_CUP = 1,
  READY_TO_START = 2,
  DRINKING = 3,
};

struct Player {
  String username = "Player";
  GameState state = IDLE;
  float baseline_weight = 0;
  float start_weight = 0;
  unsigned long drink_start = 0;
  unsigned long duration = 0;
  float drink_amount = 0;
};

AsyncWebServer server(80);
HX711 scale;
Player player;

// Кэш веса — HTTP не блокирует весы повторными чтениями
float cachedWeight = 0;

// ---------- CORS (нужно для React с localhost / телефона) ----------
void enableCors() {
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
}

// ---------- Весы ----------
void initScale() {
  scale.begin(DT_PIN, SCK_PIN);
  scale.set_scale(CALIBRATION_FACTOR);
  scale.tare();
  delay(1000);
  Serial.println("[SCALE] Ready");
}

void updateCachedWeight() {
  if (scale.is_ready()) {
    cachedWeight = scale.get_units(AVERAGE_SAMPLES);
  }
}

float readWeightFresh() {
  updateCachedWeight();
  return cachedWeight;
}

// ---------- SPIFFS / лидерборд ----------
bool initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("[SPIFFS] Mount failed");
    return false;
  }

  if (!SPIFFS.exists(LEADERBOARD_FILE)) {
    File f = SPIFFS.open(LEADERBOARD_FILE, FILE_WRITE);
    if (f) {
      f.print("[]");
      f.close();
      Serial.println("[SPIFFS] Created empty leaderboard");
    }
  }
  return true;
}

void saveResult(const String& username, unsigned long duration, float amount) {
  DynamicJsonDocument doc(4096);
  JsonArray entries;

  File f = SPIFFS.open(LEADERBOARD_FILE, FILE_READ);
  if (f) {
    deserializeJson(doc, f);
    f.close();
  }

  if (doc.is<JsonArray>()) {
    entries = doc.as<JsonArray>();
  } else if (doc["leaderboard"].is<JsonArray>()) {
    entries = doc["leaderboard"].as<JsonArray>();
  } else {
    doc.clear();
    entries = doc.to<JsonArray>();
  }

  JsonObject entry = entries.createNestedObject();
  entry["username"] = username;
  entry["duration"] = duration;
  entry["amount"]   = amount;

  for (size_t i = 0; i + 1 < entries.size(); i++) {
    for (size_t j = 0; j + 1 < entries.size() - i; j++) {
      unsigned long d1 = entries[j]["duration"].as<unsigned long>();
      unsigned long d2 = entries[j + 1]["duration"].as<unsigned long>();
      if (d1 > d2) {
        JsonObject tmp = entries[j];
        entries[j]     = entries[j + 1];
        entries[j + 1] = tmp;
      }
    }
  }

  while (entries.size() > MAX_PLAYERS) {
    entries.remove(entries.size() - 1);
  }

  f = SPIFFS.open(LEADERBOARD_FILE, FILE_WRITE);
  if (f) {
    serializeJson(entries, f);
    f.close();
  }
}

String getLeaderboardJSON() {
  File f = SPIFFS.open(LEADERBOARD_FILE, FILE_READ);
  if (!f) {
    return "[]";
  }

  String content = f.readString();
  f.close();

  if (content.length() == 0) {
    return "[]";
  }

  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, content);
  if (err) {
    return "[]";
  }

  if (doc.is<JsonArray>()) {
    String json;
    serializeJson(doc.as<JsonArray>(), json);
    return json;
  }

  if (doc["leaderboard"].is<JsonArray>()) {
    String json;
    serializeJson(doc["leaderboard"], json);
    return json;
  }

  return "[]";
}

// ---------- JSON ответы ----------
String buildStateJSON() {
  DynamicJsonDocument doc(256);
  doc["state"] = static_cast<int>(player.state);

  if (player.state == DRINKING) {
    if (player.drink_start > 0) {
      doc["duration"] = millis() - player.drink_start;
    } else {
      doc["duration"] = 0;
    }

    float liveAmount = player.start_weight - cachedWeight;
    if (liveAmount < 0) liveAmount = 0;
    doc["amount"] = (player.drink_amount > 0) ? player.drink_amount : liveAmount;
  }

  String json;
  serializeJson(doc, json);
  return json;
}

String buildWeightJSON() {
  DynamicJsonDocument doc(64);
  doc["weight"] = roundf(cachedWeight * 10.0f) / 10.0f;
  String json;
  serializeJson(doc, json);
  return json;
}

String buildInfoJSON() {
  DynamicJsonDocument doc(256);
  doc["ip"]       = WiFi.localIP().toString();
  doc["hostname"] = MDNS_HOSTNAME;
  doc["mdns"]     = String(MDNS_HOSTNAME) + ".local";
  doc["state"]    = static_cast<int>(player.state);
  String json;
  serializeJson(doc, json);
  return json;
}

// ---------- HTTP ----------
void setupRoutes() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    String page = F(
      "<!DOCTYPE html><html><head><meta charset=utf-8>"
      "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
      "<title>Beer Race ESP32</title></head><body style=\"font-family:sans-serif;background:#111;color:#eee;padding:2rem\">"
      "<h1>Beer Race ESP32-C3</h1>"
      "<p>API работает. Откройте React-приложение.</p>"
      "<ul>"
      "<li><a href=\"/info\">/info</a> — IP и статус</li>"
      "<li><a href=\"/weight\">/weight</a></li>"
      "<li><a href=\"/state\">/state</a></li>"
      "<li><a href=\"/leaderboard\">/leaderboard</a></li>"
      "</ul></body></html>");
    request->send(200, "text/html; charset=utf-8", page);
  });

  server.on("/info", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", buildInfoJSON());
  });

  server.on("/weight", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", buildWeightJSON());
  });

  server.on("/state", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", buildStateJSON());
  });

  server.on("/leaderboard", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", getLeaderboardJSON());
  });

  server.on("/api/start", HTTP_POST, [](AsyncWebServerRequest* request) {
    player.state = WAITING_CUP;
    player.drink_start = 0;
    player.duration = 0;
    player.drink_amount = 0;
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/cup", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (player.state == WAITING_CUP) {
      player.baseline_weight = getWeight();
      player.state = READY_TO_START;
      Serial.printf("[GAME] Cup placed, baseline=%.1f g\n", player.baseline_weight);
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/drink", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (player.state == READY_TO_START) {
      player.start_weight = getWeight();
      player.drink_start = 0;
      player.duration = 0;
      player.drink_amount = 0;
      player.state = DRINKING;
      Serial.printf("[GAME] Drink! start_weight=%.1f g\n", player.start_weight);
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/cancel", HTTP_POST, [](AsyncWebServerRequest* request) {
    player.state = IDLE;
    player.drink_start = 0;
    request->send(200, "text/plain", "OK");
    Serial.println("[GAME] Cancelled");
  });

  server.onNotFound([](AsyncWebServerRequest* request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
      return;
    }
    request->send(404, "text/plain", "Not found");
  });
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("[WIFI] Connecting");
  uint8_t attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 60) {
    delay(500);
    Serial.print('.');
    attempts++;
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] FAILED — проверьте SSID/пароль");
    return;
  }

  Serial.println("[WIFI] Connected");
  Serial.println("============================================");
  Serial.print("  IP address:   http://");
  Serial.println(WiFi.localIP());
  Serial.print("  mDNS:         http://");
  Serial.print(MDNS_HOSTNAME);
  Serial.println(".local");
  Serial.println("  Скопируйте IP в src/api.js → API_URL");
  Serial.println("============================================");

  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[MDNS] Started");
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("=== Beer Race ESP32-C3 ===");

  enableCors();

  if (!initSPIFFS()) {
    Serial.println("[WARN] SPIFFS init failed");
  }

  initScale();
  connectWiFi();
  setupRoutes();

  server.begin();
  Serial.println("[HTTP] Server started on port 80");

  player.state = IDLE;
}

void loop() {
  if (player.state != DRINKING) {
    delay(100);
    return;
  }

  float weight = getWeight();

  if (player.drink_start == 0) {
    float consumed = player.start_weight - weight;
    if (consumed >= DRINK_THRESHOLD) {
      player.drink_start = millis();
      player.drink_amount = consumed;
      Serial.println("[GAME] Timer started (60g threshold)");
    }
  } else {
    float consumed = player.start_weight - weight;
    if (consumed > player.drink_amount) {
      player.drink_amount = consumed;
    }

    if (weight <= player.baseline_weight + FINISH_THRESHOLD) {
      player.duration = millis() - player.drink_start;

      saveResult(player.username, player.duration, player.drink_amount);

      Serial.print("[GAME] Finished! Time: ");
      Serial.print(player.duration / 1000.0f, 2);
      Serial.print(" s, amount: ");
      Serial.print(player.drink_amount, 1);
      Serial.println(" g");

      player.state = IDLE;
      player.drink_start = 0;
    }
  }

  delay(100);
}
