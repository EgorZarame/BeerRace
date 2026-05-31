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
// const char* WIFI_SSID     = "4G-CPE_2162";
// const char* WIFI_PASSWORD = "1234567890";

const char* WIFI_SSID     = "USDBashtel_0767";
const char* WIFI_PASSWORD = "C7CAE942";

// const char* WIFI_SSID     = "iPhone (egorzzzj)";
// const char* WIFI_PASSWORD = "89870363800";

// mDNS: http://beerrace.local (работает не на всех телефонах)
const char* MDNS_HOSTNAME = "beerrace";

// ============ Пины HX711 (ESP32-C3) ============
#define DT_PIN  4
#define SCK_PIN 5

#define CALIBRATION_FACTOR  33.7f
#define DRINK_THRESHOLD     35.0f   // г — падение веса = кружку подняли
#define FINISH_THRESHOLD    30.0f   // г — верхняя граница пустой кружки
#define FINISH_WEIGHT_FLOOR -30.0f  // г — нижняя граница пустой кружки
#define MIN_POUR_WEIGHT     30.0f
#define LOOP_DELAY_MS       50
#define AVERAGE_SAMPLES     5
#define FAST_SAMPLES        3
#define FINISH_STABLE_READS 2

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
  float baseline_weight = 0;   // вес пустой кружки до tare (справочно)
  float empty_cup_weight = 0;  // вес пустой кружки после tare (≈0)
  float start_weight = 0;      // вес жидкости при DRINK
  unsigned long drink_start = 0;
  unsigned long duration = 0;
  float drink_amount = 0;
  bool cup_lifted = false;
  bool was_outside_empty_zone = false;  // вес был вне −30…+30 (кружку сняли)
  bool finish_locked = false;
  float min_weight_seen = 9999.0f;
};

AsyncWebServer server(80);
HX711 scale;
Player player;

void resetDrinkFlags() {
  player.drink_start = 0;
  player.duration = 0;
  player.drink_amount = 0;
  player.cup_lifted = false;
  player.was_outside_empty_zone = false;
  player.finish_locked = false;
  player.min_weight_seen = 9999.0f;
}

// Кэш веса — HTTP не блокирует весы повторными чтениями
float cachedWeight = 0;

// Пустая кружка: от −30 до +30 г (относительно empty_cup после tare)
bool looksLikeEmptyCup(float w) {
  float low  = player.empty_cup_weight + FINISH_WEIGHT_FLOOR;
  float high = player.empty_cup_weight + FINISH_THRESHOLD;
  return w >= low && w <= high;
}

bool isOutsideEmptyZone(float w) {
  return w < (player.empty_cup_weight + FINISH_WEIGHT_FLOOR) ||
         w > (player.empty_cup_weight + FINISH_THRESHOLD);
}

void updateCachedWeightFast() {
  if (scale.is_ready()) {
    cachedWeight = scale.get_units(FAST_SAMPLES);
  }
}

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

// Обнуление веса пустой кружки (tare)
void tareCupStable() {
  delay(400);
  for (int i = 0; i < 10; i++) {
    if (scale.is_ready()) {
      scale.get_units(5);
    }
    delay(80);
  }
  scale.tare();
  delay(300);
  updateCachedWeight();
  Serial.printf("[GAME] Cup tared, weight now %.1f g\n", cachedWeight);
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

  // Не сохранять дубликат (тот же результат подряд)
  if (entries.size() > 0) {
    JsonObject last = entries[entries.size() - 1];
    unsigned long lastDur = last["duration"].as<unsigned long>();
    float lastAmt = last["amount"].as<float>();
    if (lastDur == duration && fabsf(lastAmt - amount) < 1.0f) {
      Serial.println("[GAME] Duplicate result skipped");
      return;
    }
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
    doc["duration"] = (player.drink_start > 0) ? (millis() - player.drink_start) : 0;
    doc["amount"] = player.drink_amount;
    doc["timer_running"] = (player.drink_start > 0);
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

void finishGame(float weight) {
  if (player.finish_locked) return;
  player.finish_locked = true;
  player.state = IDLE;

  player.duration = millis() - player.drink_start;
  saveResult(player.username, player.duration, player.drink_amount);

  Serial.print("[GAME] FINISH! Time: ");
  Serial.print(player.duration / 1000.0f, 2);
  Serial.print(" s, liquid: ");
  Serial.print(player.drink_amount, 1);
  Serial.print(" g, cup: ");
  Serial.print(weight, 1);
  Serial.println(" g");

  resetDrinkFlags();
  player.start_weight = 0;
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
    resetDrinkFlags();
    player.start_weight = 0;
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/cup", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (player.state == WAITING_CUP) {
      player.baseline_weight = readWeightFresh();
      tareCupStable();
      player.empty_cup_weight = cachedWeight;
      resetDrinkFlags();
      player.start_weight = 0;
      player.state = READY_TO_START;
      Serial.printf("[GAME] Empty cup tared (was %.1f g) — pour drink\n",
                    player.baseline_weight);
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/drink", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (player.state == READY_TO_START) {
      player.start_weight = readWeightFresh();
      resetDrinkFlags();

      if (player.start_weight < MIN_POUR_WEIGHT) {
        Serial.printf("[GAME] WARN: low pour (%.1f g)\n", player.start_weight);
      }

      player.drink_amount = player.start_weight;
      player.state = DRINKING;
      Serial.printf("[GAME] DRINK armed — liquid %.1f g. Lift cup to START timer\n",
                    player.start_weight);
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/cancel", HTTP_POST, [](AsyncWebServerRequest* request) {
    player.state = IDLE;
    resetDrinkFlags();
    player.start_weight = 0;
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
  updateCachedWeight();
  connectWiFi();
  setupRoutes();

  server.begin();
  Serial.println("[HTTP] Server started on port 80");

  player.state = IDLE;
}

void loop() {
  updateCachedWeightFast();

  if (player.state != DRINKING) {
    delay(LOOP_DELAY_MS);
    return;
  }

  float weight = cachedWeight;
  static uint8_t finishStableCount = 0;

  if (weight < player.min_weight_seen) {
    player.min_weight_seen = weight;
  }

  // Ждём подъёма кружки
  if (!player.cup_lifted) {
    finishStableCount = 0;
    float drop = player.start_weight - weight;
    if (drop >= DRINK_THRESHOLD || weight <= FINISH_WEIGHT_FLOOR) {
      player.cup_lifted = true;
      player.drink_start = millis();
      player.min_weight_seen = weight;
      // Был полный стакан (>30 г) — значит кружку точно снимали
      player.was_outside_empty_zone =
        (player.start_weight > player.empty_cup_weight + FINISH_THRESHOLD);
      Serial.printf("[GAME] Cup LIFTED — timer START (weight=%.1f g)\n", weight);
    }
    delay(LOOP_DELAY_MS);
    return;
  }

  // Пока кружка снята — вес вне диапазона −30…+30
  if (isOutsideEmptyZone(weight)) {
    player.was_outside_empty_zone = true;
    finishStableCount = 0;
    delay(LOOP_DELAY_MS);
    return;
  }

  // Финиш: вернули пустую кружку (−30…+30 г), до этого вес был вне зоны
  if (player.was_outside_empty_zone && looksLikeEmptyCup(weight)) {
    finishStableCount++;
    if (finishStableCount >= FINISH_STABLE_READS) {
      finishGame(weight);
      finishStableCount = 0;
    }
  } else {
    finishStableCount = 0;
  }

  delay(LOOP_DELAY_MS);
}
