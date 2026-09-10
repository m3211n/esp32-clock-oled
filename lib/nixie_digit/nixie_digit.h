#include <vector>
#include <array>
#include <cstdint>

#ifndef NIXIE_DIGIT_H
#define NIXIE_DIGIT_H

namespace NixieDigit {

// "Nixie digits" grid
// Grid only works for displays with 15:4 ratio (e.g. 128x32), rotated vertically.
// The grid is a 27 points (9 x 3) layout, with each point spaced apart by equal amount of pixels.
//
//                0--1--2--3--4--5--6--7--8
//                |  |  |  |  |  |  |  |  |
//                9--10-11-12-13-14-15-16-17
//                |  |  |  |  |  |  |  |  |
//                18-19-20-21-22-23-24-25-26
//         
// Each point in the grid corresponds to a specific pixel position on the display.
// The points are defined in a way that allows for easy drawing of digits using connected lines.
//
// For example, number 3 would be drawn by connecting consecutive dots 0, 9, 20, 23, 14, 25, 26, 8, 7 
// as follows:
//
//                0--+--+--+--+--+--+--7--8
//                |  |  |  |  |  |  |  |  |
//                9--+--+--+--+--14-+--+--+ 
//                |  |  |  |  |  |  |  |  |
//                +--+--20-+--+--23-+--25-26
//
// The display is rotated 90 degrees counter-clockwise, so the grid is horizontal.

    // This is all digits' points that eventually will be "unpacked" to line coordinates (x, y)
    static const std::array<std::vector<uint8_t>, 10> POINTS = {{
        {0, 8, 26, 18, 0},                  // 0
        {7, 17, 9},                         // 1
        {18, 0, 1, 23, 26, 8, 7},           // 2
        {0, 9, 20, 23, 14, 25, 26, 8, 7},   // 3
        {18, 26, 4, 3, 21},                 // 4
        {0, 9, 20, 23, 5, 8, 26},           // 5
        {5, 23, 18, 0, 6, 17, 26},          // 6
        {0, 26, 8, 7},                      // 7
        {0, 2, 24, 26, 8, 6, 20, 18, 0},    // 8
        {0, 9, 20, 26, 8, 3, 21}            // 9
    }};

    // This is how many points each grid "line" has. 
    constexpr uint8_t GRID_WIDTH = 9;

    // This is how many pixels are in between on each point both horizontally and 
    // vertically; this value is used to "unpack" point number to the (x, y)
    // coordinates of the line. For SSD1306 this will be 15.

    struct Line { uint16_t x0, y0, x1, y1; };

    using Digit = std::vector<Line>;
    using DigitSet = std::array<Digit, 10>;

    void unpackLines(DigitSet& digits, uint16_t canvasWidth);
}

#endif