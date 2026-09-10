#include <array>
#include <Arduino.h>

#include "Adafruit_SSD1306.h"
#include <time.h>
#include "nixie_digit.h"

#ifndef NIXIE_CLOCK_H
#define NIXIE_CLOCK_H

#define VERBOSE_MODE 1

// I2C hardware parameters for the I2C multiplexor (TCA9548 / PCA9548)
constexpr uint8_t MUX_ADDR = 0x70;
constexpr uint32_t I2C_SPEED = 100000; // Hz
// Default ESP32-C3 I2C pins
constexpr uint8_t I2C_SDA_PIN = 8;
constexpr uint8_t I2C_SCL_PIN = 9;
// This is how many displays are connected to the mux
constexpr uint8_t CLUSTER_SIZE = 6; // Max: 8
// Display (SSD1306 OLED) parameters
constexpr uint8_t DISPLAY_ADDR = 0x3C; // < See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
constexpr int16_t DISPLAY_WIDTH = 128; // SSD1306 screen width
constexpr int16_t DISPLAY_HEIGHT = 32; // SSD1306 screen height

constexpr int16_t DISPLAY_BLINK_INTERVAL = 250; // ms
constexpr int16_t CLOCK_UPDATE_INTERVAL = 1000; // ms


namespace NixieClock {
    using DigitRegister = std::array<char, CLUSTER_SIZE>;

    struct DisplayRegister {
        uint8_t digit;
        bool focused;
    };

    // Multiplexer with displays connected to its channels
    class MultiDisplay : public Adafruit_SSD1306 {
    public:
        MultiDisplay();
        // Initializes multiplexor and displays
        bool begin();
        // Updates the digit in the registar if it was changed and paints in on the display
        bool setDigit(uint8_t position, uint8_t digit);
        // Changes focused state of the display at specific channel
        void focus(uint8_t position, bool focused);
        // Returns focused state
        bool isFocused(uint8_t);
        // Selects a channel with connected display and draws a digit in it
        void refresh(uint8_t position);
        // Draws the entire buffer to all displays consequentially
        bool scan();


    private:
        bool selectChannel_(uint8_t channel);
        // Clocks SCL to release a stuck SDA line (I2C bus recovery)
        void recoverBus_();
        // Probes the multiplexer address (with retries); returns true if it ACKs
        bool detectMux_();
        // Scans the whole bus and prints every address that ACKs (diagnostic)
        void scanBus_();

        uint8_t  size_;
        uint8_t  mux_addr_;
        uint8_t  display_addr_;
        uint32_t bus_speed_;
        uint8_t  currentChannel_;
        bool     ready_ = false; // Set only after begin() fully succeeds

        // NixieClock::DigitRegister digitRegister_;
        NixieDigit::DigitSet linesUnpacked_;
        std::array<NixieClock::DisplayRegister, CLUSTER_SIZE> displayRegister_;
    };

    // Clock manager
    class Clock {
    public:
        Clock();

        bool begin(time_t initTime);
        // Sends new date and time to RTC and updates the displays
        void refresh();
        // Switches clock mode between time and date
        void switchMode(bool showTime);

    private:
        time_t now_;
        bool updateClock_;
        bool mode_;

        NixieClock::MultiDisplay multiDisplay_;

        void updateDisplaysRegister_();
        void updateDisplayPair_(uint8_t index, uint8_t value);
    };
}

#endif