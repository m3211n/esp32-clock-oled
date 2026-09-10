#include "nixie_clock.h"

namespace NixieClock {

    Clock::Clock() {
        mode_ = true;
        updateClock_ = false;
    }

    bool Clock::begin(time_t initTime) {

        Serial.println("*** Nixie Clock begin...");

        now_ = initTime;
        if (!multiDisplay_.begin()) {
            Serial.println("[ERROR] Multi-display initialization failed!");
            return false;
        }
        Serial.println("*** Nixie Clock: OK!");
        return true;
    }

    void Clock::switchMode(bool mode) {

        Serial.printf("[ACTION] Switching mode to %s \n", mode_ ? "time" : "date");

        mode_ = mode;
    }

    void Clock::updateDisplayPair_(uint8_t index, uint8_t value) {
        multiDisplay_.setDigit(index * 2, value / 10);
        multiDisplay_.setDigit(index * 2 + 1, value % 10);
    };

    void Clock::updateDisplaysRegister_() {

        Serial.println("[INFO] Updating display registers...");

        struct tm timeinfo;
        localtime_r(&now_, &timeinfo);

        const uint16_t year  = timeinfo.tm_year + 1900;
        const uint8_t  month = timeinfo.tm_mon + 1;
        const uint8_t  day   = timeinfo.tm_mday;
        const uint8_t  hour  = timeinfo.tm_hour;
        const uint8_t  min   = timeinfo.tm_min;
        const uint8_t  sec   = timeinfo.tm_sec;

        updateDisplayPair_(0, mode_ ? hour : year % 100);
        updateDisplayPair_(1, mode_ ? min : month);
        updateDisplayPair_(2, mode_ ? sec : day);

        Serial.println("[INFO] Display registers updated...");

    }

    void Clock::refresh() {
        now_ = time(nullptr);

        Serial.printf("[INFO] Clock refresh. Current mode is %s \n", mode_ ? "time" : "date");

        updateDisplaysRegister_();
    }

    ////////////////////////////////////////////////////////////////////////////

    MultiDisplay::MultiDisplay() : Adafruit_SSD1306(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, -1) {
        mux_addr_ =     MUX_ADDR;
        display_addr_ = DISPLAY_ADDR;
        size_ =         CLUSTER_SIZE;
        bus_speed_ =    I2C_SPEED;
        currentChannel_ = 8; // Initialize with value bigger than 7 (maximum channel address)
    }

    bool MultiDisplay::begin() {

        Serial.println("*** Multi-display begin...");

        // Unpack digit points to draw lines
        NixieDigit::unpackLines(linesUnpacked_, DISPLAY_WIDTH);

        // Prepare I2C bus (default ESP32-C3 pins: SDA=8, SCL=9)
        Serial.printf("[I2C] Initializing bus on SDA=%d SCL=%d @ %lu Hz\n",
                      I2C_SDA_PIN, I2C_SCL_PIN, (unsigned long)bus_speed_);
        Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
        Wire.setClock(bus_speed_);

        // NOTE: recoverBus_() is disabled. It drives the I2C pins directly with
        // pinMode()/digitalWrite() after Wire.begin(), which fights the ESP32
        // I2C peripheral and can wedge the bus (caused mux probe timeouts + a
        // hung scan). The Wire library's own 50ms transaction timeout is enough.
        // recoverBus_();

        // Verify the multiplexer is actually present before touching the displays
        if (!detectMux_()) {
            Serial.printf("[ERROR] Multiplexer (0x%02X) not found on I2C. Check wiring/power/pins.\n", mux_addr_);
            return false;
        }
        Serial.printf("[I2C] Multiplexer (0x%02X) detected.\n", mux_addr_);

        // Initialize SSD1306 OLEDs
        for (uint8_t chan = 0; chan < size_; chan++) {
            if (!selectChannel_(chan)) {
                Serial.printf("[ERROR] Failed to select channel %d\n", chan);
                return false;
            }
            Serial.printf("[I2C] Initializing display on channel %d (addr 0x%02X)...\n", chan, DISPLAY_ADDR);
            if (!Adafruit_SSD1306::begin(i2caddr=DISPLAY_ADDR)) {
                Serial.printf("[ERROR] SSD1306 on channel %d did not respond.\n", chan);
                return false;
            }
        }

        Serial.println("*** Multi display: OK!");

        ready_ = true;

        // Force-draw "0" on every display so the user sees 00:00:00 right
        // away (the register already holds 0, so setDigit() would skip it).
        for (uint8_t chan = 0; chan < size_; chan++) {
            displayRegister_[chan].digit = 0;
            refresh(chan);
        }

        return scan();
    }

    void MultiDisplay::recoverBus_() {
        // If SDA is stuck low, clock SCL up to 9 times to let the slave release it.
        pinMode(I2C_SDA_PIN, INPUT_PULLUP);
        pinMode(I2C_SCL_PIN, INPUT_PULLUP);
        if (digitalRead(I2C_SDA_PIN) == LOW) {
            Serial.println("[I2C] SDA stuck low - attempting bus recovery...");
            pinMode(I2C_SCL_PIN, OUTPUT);
            for (int i = 0; i < 9; i++) {
                digitalWrite(I2C_SCL_PIN, HIGH);
                delayMicroseconds(5);
                digitalWrite(I2C_SCL_PIN, LOW);
                delayMicroseconds(5);
            }
            // Issue a STOP condition to reset the bus
            pinMode(I2C_SDA_PIN, OUTPUT);
            digitalWrite(I2C_SDA_PIN, LOW);
            digitalWrite(I2C_SCL_PIN, HIGH);
            digitalWrite(I2C_SDA_PIN, HIGH);
            pinMode(I2C_SDA_PIN, INPUT_PULLUP);
            pinMode(I2C_SCL_PIN, INPUT_PULLUP);
            Serial.println("[I2C] Bus recovery complete.");
        }
    }

    bool MultiDisplay::detectMux_() {
        // Retry a few times - the bus can be flaky right after boot/WiFi init
        for (uint8_t attempt = 1; attempt <= 3; attempt++) {
            Wire.beginTransmission(mux_addr_);
            uint8_t result = Wire.endTransmission();
            if (result == 0) {
                return true;
            }
            Serial.printf("[I2C] Mux probe attempt %d failed, error code %d (ESP32: 1=addr NACK, 2=data NACK, 5=timeout)\n", attempt, result);
            delay(100);
        }
        // Diagnostic: show what IS on the bus
        scanBus_();
        return false;
    }

    void MultiDisplay::scanBus_() {
        Serial.println("[I2C] --- Bus scan (diagnostic) ---");
        uint8_t found = 0;
        for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0) {
                Serial.printf("[I2C]   0x%02X ACKs\n", addr);
                found++;
            }
        }
        if (found == 0) Serial.println("[I2C]   (nothing found)");
        Serial.println("[I2C] --- End of bus scan ---");
    }

    bool MultiDisplay::selectChannel_(uint8_t channel) {
        if (channel > size_ - 1) return false;
        Wire.beginTransmission(mux_addr_);
        Wire.write(1 << channel); // Send bitmask to the control register
        // If transmission sucessful, record current channel
        if (Wire.endTransmission() == 0) {
            currentChannel_ = channel;

            #if VERBOSE_MODE > 0
                Serial.printf("Mux >> %02d.\n", currentChannel_);
            #endif

            return true;
        } else return false;
    }

    bool MultiDisplay::setDigit(uint8_t position, uint8_t value) {
        if (!ready_) return false; // Display not initialized - skip to avoid NULL buffer access
        if (displayRegister_[position].digit != value) {

            Serial.printf("Assigning %d to position %02d (current value: %d) \n", value, position, displayRegister_[position].digit);

            displayRegister_[position].digit = value;
            refresh(position);
            return true;
        } else return false;
    }

    void MultiDisplay::refresh(uint8_t position) {
        if (!ready_) return; // Display not initialized - framebuffer is NULL
        // Implementation to update display with digit
        const auto& lines = linesUnpacked_[displayRegister_[position].digit];
        selectChannel_(position);
        clearDisplay();

        Serial.printf("Printing %d at position %02d \n", displayRegister_[position].digit, position);
        
        for (auto& line : lines) { 
            drawLine(line.x0 + 6, line.y0, line.x1 + 6, line.y1, SSD1306_WHITE);
        }
        
        // invertDisplay(displayRegister_[position].focused);
        fillRect(0, 0, 3, 31, displayRegister_[position].focused ? SSD1306_WHITE : SSD1306_BLACK);

        dim(!displayRegister_[position].focused);
        // setRotation(1);
        // drawChar(0, 0, displayRegister_[position].value + '0', 1, 0, 6);
        display();
    }

    void MultiDisplay::focus(uint8_t position, bool focused) {
        displayRegister_[position].focused = focused;
        // paint horizontal lines with black to dim even more;
    }

    bool MultiDisplay::isFocused(uint8_t position) {
        return displayRegister_[position].focused;
    }

    bool MultiDisplay::scan() {
        // In verbose mode, adds a delay 5 sec. to give time for cable re-connection, running terminal etc.
        for (uint8_t chan = 0; chan < size_; chan++) {
            if (!selectChannel_(chan)) {
                Serial.println("[ERROR] Could not connect to Multiplexer!");
                return false;
            }
            uint8_t connectedDevices = 0;
            for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
            // Ignore the Mux's address
                if (addr == mux_addr_) continue;
                Wire.beginTransmission(addr);
                if (Wire.endTransmission() == 0) {
                    connectedDevices++;

                    #if VERBOSE_MODE > 0
                    Serial.printf("Found I2C device (%#x) at channel %02d\n", addr, chan);
                    #endif

                }
            }

            #if VERBOSE_MODE > 0
            if (connectedDevices == 0) { Serial.printf("No I2C devices found at channel %02d\n", chan); }
            #endif

        }
        selectChannel_(0);
        return true;
    }
}

