#define VERBOSE_MODE 1

#include "nixie_clock.h"
#include "WiFi.h"
#include "time.h"
#include "Adafruit_NeoPixel.h"

// --- RGB status LED (WS2812 / addressable) on GPIO10 ---
constexpr uint8_t STATUS_LED_PIN = 10;
constexpr uint8_t STATUS_LED_COUNT = 1;
constexpr uint8_t STATUS_LED_BRIGHTNESS = 40;

Adafruit_NeoPixel statusLed(STATUS_LED_COUNT, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);

// Status LED helpers
void ledSolid(uint8_t r, uint8_t g, uint8_t b) {
  statusLed.setPixelColor(0, statusLed.Color(r, g, b));
  statusLed.show();
}

void ledBlink(uint8_t r, uint8_t g, uint8_t b, uint32_t durationMs) {
  uint32_t start = millis();
  while (millis() - start < durationMs) {
    statusLed.setPixelColor(0, statusLed.Color(r, g, b));
    statusLed.show();
    delay(250);
    statusLed.setPixelColor(0, 0);
    statusLed.show();
    delay(250);
  }
}

void ledOff() {
  statusLed.setPixelColor(0, 0);
  statusLed.show();
}

// Timezone: CET/CEST (Europe). CET = UTC+1, CEST (DST) = UTC+2.
// Equivalent POSIX TZ string: "CET-1CEST,M3.5.0/2,M10.5.0/3"

volatile bool showTime = true;
volatile uint8_t focus = 0;
bool clockOk = false;  // Set in setup() once the display is ready
bool timeSynced = false; // Set in setup() once NTP time is valid

const char* SSID = "Home Sweet Home 6G_IoT";
const char* PASS = "12345678";

NixieClock::Clock nixieClock;

void toggleFocus() {
  if (focus == 2) { focus = 0; } 
  else if (focus == 0) { focus = 1; } 
  else if (focus == 1) { focus = 2; }
}

void setup() {
  Serial.begin(115200);
  delay(1000); // Give the serial monitor time to attach before printing

  Serial.println("[INFO] ---- NIXIE CLOCK INITIALIZATION SEQUENCE START ----");

  // Initialize the RGB status LED
  statusLed.begin();
  statusLed.setBrightness(STATUS_LED_BRIGHTNESS);

  // --- STEP 0: Green solid = powered on, attempting to connect & sync ---
  ledSolid(255, 0, 0);

  // --- Initialize the display NOW so it shows 00:00:00 while we wait for
  // --- WiFi + NTP. The main loop renders the real time once it's synced.
  clockOk = nixieClock.begin(0);
  if (!clockOk) {
    Serial.println("[ERROR] Display init failed - display updates disabled.");
  }

  // --- STEP 1: Connect to WiFi (retry loop, red blinking on failure) ---
  bool wifiOk = false;
  while (!wifiOk) {
    Serial.printf("Connecting to %s ", SSID);
    WiFi.begin(SSID, PASS);

    // Wait for WiFi to connect (with timeout so we never hang silently)
    uint32_t wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 20000) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      wifiOk = true;
      Serial.printf("Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
      // Red blinking = WiFi connection problem; retry after a pause
      Serial.println("\n[ERROR] WiFi connection failed. Retrying in 5s...");
      ledBlink(0, 255, 0, 5000);
    }
  }

  // --- STEP 2: Start NTP Sync ---
  // On ESP32, configTime() starts the SNTP client and applies the timezone.
  // gmtOffset_sec = 3600 (CET, UTC+1); daylightOffset_sec = 3600 (CEST adds +1h).
  Serial.println("Starting NTP sync...");
  configTime(3600, 3600, "pool.ntp.org", "time.nist.gov");

  // --- STEP 3: Wait for Time (retry loop, yellow solid on failure) ---
  bool ntpOk = false;
  while (!ntpOk) {
    Serial.print("Waiting for NTP response");
    struct tm timeinfo;
    uint32_t ntpStart = millis();
    while (!getLocalTime(&timeinfo) && millis() - ntpStart < 30000) {
      delay(500);
      Serial.print(".");
    }
    if (getLocalTime(&timeinfo)) {
      ntpOk = true;
      Serial.println(" -> Time Received!");
    } else {
      // Yellow solid = NTP sync problem; retry after a pause
      Serial.println("\n[ERROR] NTP sync failed. Retrying in 5s...");
      ledSolid(0, 255, 0);
      delay(5000);
    }
  }

  // --- STEP 4: Green off = time sync successful ---
  ledOff();

  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) { // Wait until we have a realistic time
      delay(500);
      Serial.print(".");
      now = time(nullptr);
  }
  Serial.println("\nTime Synced!");
  timeSynced = true;

  Serial.println("[INFO] ---- NIXIE CLOCK INITIALIZATION SEQUENCE DONE! ----");
}

unsigned long lastUpdate = 0;

void loop() {
  if (!clockOk) {
    // Keep blinking red while the display is not ready
    ledBlink(0, 255, 0, 1000);
    return;
  }
  // Until NTP time is valid, leave the 00:00:00 placeholder on the displays
  if (!timeSynced) {
    return;
  }
  if (millis() - lastUpdate >= CLOCK_UPDATE_INTERVAL) {
    lastUpdate = millis();
    // nixieClock.switchMode(showTime);
    // nixieClock.focusAt(focus);
    Serial.println("[INFO] Main loop update triggered!");
    nixieClock.refresh();
  }
}

