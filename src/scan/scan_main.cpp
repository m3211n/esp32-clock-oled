// Standalone I2C bus scanner for the Nixie Clock project.
// Scans the base I2C bus (SDA=8, SCL=9) and, if a TCA9548/PCA9548
// multiplexer is found (0x70-0x77), scans all 8 of its channels.
// Re-scans every 10 seconds so you can rewire while it's running.

#include <Arduino.h>
#include <Wire.h>

constexpr uint8_t I2C_SDA_PIN = 8;
constexpr uint8_t I2C_SCL_PIN = 9;
constexpr uint32_t SCAN_INTERVAL_MS = 10000;

static void scanBaseBus() {
  Serial.println("--- Base bus (SDA=8, SCL=9) ---");
  uint8_t found = 0;
  for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  0x%02X  %s\n", addr,
                    (addr >= 0x70 && addr <= 0x77) ? "<- I2C mux (TCA9548/PCA9548)" : "");
      found++;
    }
  }
  if (found == 0) Serial.println("  (nothing found)");
}

static void scanMuxChannels(uint8_t muxAddr) {
  Serial.printf("--- Mux at 0x%02X: channel scan ---\n", muxAddr);
  for (uint8_t chan = 0; chan < 8; chan++) {
    Wire.beginTransmission(muxAddr);
    Wire.write(1 << chan); // enable only this channel
    if (Wire.endTransmission() != 0) {
      Serial.printf("  CH%d: mux NACK\n", chan);
      continue;
    }
    delay(10);
    uint8_t devCount = 0;
    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
      if (addr == muxAddr) continue;
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        Serial.printf("  CH%d: 0x%02X%s\n", chan, addr,
                      (addr == 0x3C || addr == 0x3D) ? "  <- SSD1306 OLED" : "");
        devCount++;
      }
    }
    if (devCount == 0) Serial.printf("  CH%d: (empty)\n", chan);
  }
  // Re-enable all channels (default state)
  Wire.beginTransmission(muxAddr);
  Wire.write(0xFF);
  Wire.endTransmission();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);
  Serial.println("=== I2C Scanner (re-scans every 10s) ===");
}

void loop() {
  Serial.println();
  scanBaseBus();
  for (uint8_t muxAddr = 0x70; muxAddr <= 0x77; muxAddr++) {
    Wire.beginTransmission(muxAddr);
    if (Wire.endTransmission() == 0) {
      scanMuxChannels(muxAddr);
      break;
    }
  }
  delay(SCAN_INTERVAL_MS);
}