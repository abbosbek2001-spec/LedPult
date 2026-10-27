// ESP32-C3 + WS2812B — WiFi orqali boshqaruv (LedPult ilovasi uchun)
// Kutubxona: Adafruit NeoPixel (Library Manager orqali o'rnating)
// Plata: "ESP32C3 Dev Module", USB CDC On Boot: Enabled

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>

// ====== SOZLAMALAR ======
const char* WIFI_SSID = "WIFI_NOMI";     // uy WiFi nomi
const char* WIFI_PASS = "WIFI_PAROLI";   // uy WiFi paroli
const char* AP_SSID   = "LED-ESP32";     // WiFi topilmasa o'zi tarqatadi
const char* AP_PASS   = "12345678";      // (IP: 192.168.4.1)
#define LED_PIN   4                      // WS2812B DIN ulangan GPIO
#define MAX_LEDS  300                    // maksimal LED soni
// ========================

Adafruit_NeoPixel strip(MAX_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);
Preferences prefs;

bool     power = true;
uint8_t  r = 255, g = 0, b = 0;
uint8_t  bright = 128;      // 0..255
uint8_t  mode = 0;          // 0 statik, 1 kamalak, 2 nafas, 3 yuguruvchi
uint8_t  speed = 50;        // 1..100
uint16_t count = 30;        // LED soni

uint32_t lastFrame = 0, saveAt = 0;
uint16_t step = 0;

void loadSettings() {
  prefs.begin("led", true);
  power  = prefs.getBool("p", true);
  r      = prefs.getUChar("r", 255);
  g      = prefs.getUChar("g", 0);
  b      = prefs.getUChar("b", 0);
  bright = prefs.getUChar("br", 128);
  mode   = prefs.getUChar("m", 0);
  speed  = prefs.getUChar("s", 50);
  count  = prefs.getUShort("c", 30);
  prefs.end();
  if (count < 1 || count > MAX_LEDS) count = 30;
}

void saveSettings() {
  prefs.begin("led", false);
  prefs.putBool("p", power);
  prefs.putUChar("r", r);  prefs.putUChar("g", g);  prefs.putUChar("b", b);
  prefs.putUChar("br", bright);
  prefs.putUChar("m", mode);
  prefs.putUChar("s", speed);
  prefs.putUShort("c", count);
  prefs.end();
}

void cors() { server.sendHeader("Access-Control-Allow-Origin", "*"); }

String stateJson() {
  char buf[200];
  snprintf(buf, sizeof(buf),
    "{\"power\":%d,\"r\":%u,\"g\":%u,\"b\":%u,\"br\":%u,\"mode\":%u,\"speed\":%u,\"count\":%u,\"max\":%u}",
    power ? 1 : 0, r, g, b, bright, mode, speed, count, MAX_LEDS);
  return String(buf);
}

void handleState() { cors(); server.send(200, "application/json", stateJson()); }

void handleSet() {
  if (server.hasArg("power")) power  = server.arg("power").toInt() != 0;
  if (server.hasArg("r"))     r      = constrain(server.arg("r").toInt(), 0, 255);
  if (server.hasArg("g"))     g      = constrain(server.arg("g").toInt(), 0, 255);
  if (server.hasArg("b"))     b      = constrain(server.arg("b").toInt(), 0, 255);
  if (server.hasArg("br"))    bright = constrain(server.arg("br").toInt(), 0, 255);
  if (server.hasArg("mode"))  mode   = constrain(server.arg("mode").toInt(), 0, 3);
  if (server.hasArg("speed")) speed  = constrain(server.arg("speed").toInt(), 1, 100);
  if (server.hasArg("count")) count  = constrain(server.arg("count").toInt(), 1, MAX_LEDS);
  saveAt = millis() + 2000;   // 2 soniyadan keyin xotiraga yoziladi
  cors();
  server.send(200, "application/json", stateJson());
}

void render() {
  strip.clear();
  if (power) {
    strip.setBrightness(bright);
    switch (mode) {
      case 0:  // statik
        for (uint16_t i = 0; i < count; i++) strip.setPixelColor(i, r, g, b);
        break;
      case 1:  // kamalak
        for (uint16_t i = 0; i < count; i++)
          strip.setPixelColor(i, strip.gamma32(strip.ColorHSV((i * 65536UL / count) + step * 256)));
        break;
      case 2: { // nafas
        uint8_t k = (step % 512 < 256) ? (step % 256) : (255 - step % 256);
        k = max<uint8_t>(k, 8);
        for (uint16_t i = 0; i < count; i++)
          strip.setPixelColor(i, r * k / 255, g * k / 255, b * k / 255);
        break;
      }
      case 3: { // yuguruvchi
        uint16_t head = step % count;
        for (uint8_t t = 0; t < 5 && t < count; t++) {
          uint16_t i = (head + count - t) % count;
          uint8_t k = 255 >> t;
          strip.setPixelColor(i, r * k / 255, g * k / 255, b * k / 255);
        }
        break;
      }
    }
  }
  strip.show();
}

void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi'ga ulanmoqda");
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(300); Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nUlandi! IP: ");
    Serial.println(WiFi.localIP());
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.print("\nWiFi topilmadi. AP rejimi: ");
    Serial.print(AP_SSID); Serial.print("  IP: ");
    Serial.println(WiFi.softAPIP());
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  loadSettings();
  strip.begin();
  strip.show();
  setupWifi();

  server.on("/state", handleState);
  server.on("/set", handleSet);
  server.on("/", []() { cors(); server.send(200, "text/plain", "LED ESP32 ishlayapti"); });
  server.begin();
}

void loop() {
  server.handleClient();

  uint16_t interval = map(speed, 1, 100, 120, 5);   // ms
  if (millis() - lastFrame >= interval) {
    lastFrame = millis();
    step++;
    render();
  }

  if (saveAt && millis() > saveAt) { saveAt = 0; saveSettings(); }

  // WiFi uzilsa qayta ulanish
  static uint32_t lastCheck = 0;
  if (WiFi.getMode() == WIFI_STA && millis() - lastCheck > 10000) {
    lastCheck = millis();
    if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();
  }
}
