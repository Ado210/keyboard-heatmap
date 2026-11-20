#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <windows.h>
#include <iostream>
#include <vector>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <string>
#include <thread>
#include <atomic>
#include <algorithm>
#include <filesystem>

struct KeyRect {
    uint16_t sc;
    float x;
    float y;
    float w;
    float h;
    const char* label;
};

struct RGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

void put_pixel(std::vector<uint8_t>& img, int w, int h, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || x >= w || y < 0 || y >= h) return;
    int idx = (y * w + x) * 3;
    img[idx]     = r;
    img[idx + 1] = g;
    img[idx + 2] = b;
}

void blend_pixel(std::vector<uint8_t>& img, int w, int h, int x, int y, uint8_t r, uint8_t g, uint8_t b, float a) {
    if (x < 0 || x >= w || y < 0 || y >= h || a <= 0.0f) return;
    if (a > 1.0f) a = 1.0f;
    int idx = (y * w + x) * 3;
    float inv = 1.0f - a;
    img[idx]     = static_cast<uint8_t>(img[idx]     * inv + r * a);
    img[idx + 1] = static_cast<uint8_t>(img[idx + 1] * inv + g * a);
    img[idx + 2] = static_cast<uint8_t>(img[idx + 2] * inv + b * a);
}

void fill_rect(std::vector<uint8_t>& img, int w, int h, int x0, int y0, int rw, int rh, uint8_t r, uint8_t g, uint8_t b) {
    for (int y = y0; y < y0 + rh; ++y) {
        for (int x = x0; x < x0 + rw; ++x) {
            put_pixel(img, w, h, x, y, r, g, b);
        }
    }
}

void fill_round_rect(std::vector<uint8_t>& img, int w, int h, int x0, int y0, int rw, int rh, int rad, uint8_t r, uint8_t g, uint8_t b) {
    for (int y = 0; y < rh; ++y) {
        for (int x = 0; x < rw; ++x) {
            int dx = 0;
            int dy = 0;
            if (x < rad) dx = rad - x;
            else if (x >= rw - rad) dx = x - (rw - rad - 1);
            if (y < rad) dy = rad - y;
            else if (y >= rh - rad) dy = y - (rh - rad - 1);

            if (dx * dx + dy * dy <= rad * rad) {
                put_pixel(img, w, h, x0 + x, y0 + y, r, g, b);
            }
        }
    }
}

void stroke_round_rect(std::vector<uint8_t>& img, int w, int h, int x0, int y0, int rw, int rh, int rad, uint8_t r, uint8_t g, uint8_t b) {
    int r_out2 = rad * rad;
    int r_in = (rad > 1) ? rad - 1 : 0;
    int r_in2 = r_in * r_in;

    for (int y = 0; y < rh; ++y) {
        for (int x = 0; x < rw; ++x) {
            int dx = 0;
            int dy = 0;
            if (x < rad) dx = rad - x;
            else if (x >= rw - rad) dx = x - (rw - rad - 1);
            if (y < rad) dy = rad - y;
            else if (y >= rh - rad) dy = y - (rh - rad - 1);

            int d2 = dx * dx + dy * dy;
            if (d2 <= r_out2 && (d2 > r_in2 || x == 0 || x == rw - 1 || y == 0 || y == rh - 1)) {
                put_pixel(img, w, h, x0 + x, y0 + y, r, g, b);
            }
        }
    }
}

const uint8_t* get_glyph(char c) {
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
    switch (c) {
        case 'A': { static const uint8_t g[7] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}; return g; }
        case 'B': { static const uint8_t g[7] = {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}; return g; }
        case 'C': { static const uint8_t g[7] = {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}; return g; }
        case 'D': { static const uint8_t g[7] = {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C}; return g; }
        case 'E': { static const uint8_t g[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}; return g; }
        case 'F': { static const uint8_t g[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}; return g; }
        case 'G': { static const uint8_t g[7] = {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}; return g; }
        case 'H': { static const uint8_t g[7] = {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}; return g; }
        case 'I': { static const uint8_t g[7] = {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}; return g; }
        case 'J': { static const uint8_t g[7] = {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}; return g; }
        case 'K': { static const uint8_t g[7] = {0x11,0x12,0x14,0x18,0x14,0x12,0x11}; return g; }
        case 'L': { static const uint8_t g[7] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}; return g; }
        case 'M': { static const uint8_t g[7] = {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}; return g; }
        case 'N': { static const uint8_t g[7] = {0x11,0x19,0x15,0x13,0x11,0x11,0x11}; return g; }
        case 'O': { static const uint8_t g[7] = {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}; return g; }
        case 'P': { static const uint8_t g[7] = {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}; return g; }
        case 'Q': { static const uint8_t g[7] = {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}; return g; }
        case 'R': { static const uint8_t g[7] = {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}; return g; }
        case 'S': { static const uint8_t g[7] = {0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E}; return g; }
        case 'T': { static const uint8_t g[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}; return g; }
        case 'U': { static const uint8_t g[7] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}; return g; }
        case 'V': { static const uint8_t g[7] = {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}; return g; }
        case 'W': { static const uint8_t g[7] = {0x11,0x11,0x11,0x15,0x15,0x1B,0x11}; return g; }
        case 'X': { static const uint8_t g[7] = {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}; return g; }
        case 'Y': { static const uint8_t g[7] = {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}; return g; }
        case 'Z': { static const uint8_t g[7] = {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}; return g; }
        case '0': { static const uint8_t g[7] = {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}; return g; }
        case '1': { static const uint8_t g[7] = {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}; return g; }
        case '2': { static const uint8_t g[7] = {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}; return g; }
        case '3': { static const uint8_t g[7] = {0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E}; return g; }
        case '4': { static const uint8_t g[7] = {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}; return g; }
        case '5': { static const uint8_t g[7] = {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}; return g; }
        case '6': { static const uint8_t g[7] = {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}; return g; }
        case '7': { static const uint8_t g[7] = {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}; return g; }
        case '8': { static const uint8_t g[7] = {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}; return g; }
        case '9': { static const uint8_t g[7] = {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}; return g; }
        case '-': { static const uint8_t g[7] = {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}; return g; }
        case '=': { static const uint8_t g[7] = {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00}; return g; }
        case '+': { static const uint8_t g[7] = {0x00,0x04,0x04,0x1F,0x04,0x04,0x00}; return g; }
        case '*': { static const uint8_t g[7] = {0x00,0x15,0x0E,0x1F,0x0E,0x15,0x00}; return g; }
        case '/': { static const uint8_t g[7] = {0x01,0x02,0x02,0x04,0x08,0x08,0x10}; return g; }
        case '\\': { static const uint8_t g[7] = {0x10,0x08,0x08,0x04,0x02,0x02,0x01}; return g; }
        case '[': { static const uint8_t g[7] = {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}; return g; }
        case ']': { static const uint8_t g[7] = {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}; return g; }
        case ';': { static const uint8_t g[7] = {0x00,0x04,0x04,0x00,0x04,0x04,0x08}; return g; }
        case '\'': { static const uint8_t g[7] = {0x04,0x04,0x08,0x00,0x00,0x00,0x00}; return g; }
        case ',': { static const uint8_t g[7] = {0x00,0x00,0x00,0x00,0x04,0x04,0x08}; return g; }
        case '.': { static const uint8_t g[7] = {0x00,0x00,0x00,0x00,0x00,0x06,0x06}; return g; }
        case '`': { static const uint8_t g[7] = {0x08,0x04,0x02,0x00,0x00,0x00,0x00}; return g; }
        case '^': { static const uint8_t g[7] = {0x04,0x0A,0x11,0x00,0x00,0x00,0x00}; return g; }
        case 'v': { static const uint8_t g[7] = {0x00,0x00,0x00,0x00,0x11,0x0A,0x04}; return g; }
        case '<': { static const uint8_t g[7] = {0x02,0x04,0x08,0x10,0x08,0x04,0x02}; return g; }
        case '>': { static const uint8_t g[7] = {0x08,0x04,0x02,0x01,0x02,0x04,0x08}; return g; }
        default:  { static const uint8_t g[7] = {0,0,0,0,0,0,0}; return g; }
    }
}

void draw_text_centered(std::vector<uint8_t>& img, int w, int h, int rx, int ry, int rw, int rh, int scale, const char* text, uint8_t r, uint8_t g, uint8_t b) {
    int len = 0;
    while (text[len] != '\0') ++len;
    if (len == 0) return;

    int tw = (len * 6 - 1) * scale;
    int th = 7 * scale;
    int cx = rx + (rw - tw) / 2;
    int cy = ry + (rh - th) / 2;

    for (int i = 0; i < len; ++i) {
        const uint8_t* glyph = get_glyph(text[i]);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (glyph[row] & (1 << (4 - col))) {
                    fill_rect(img, w, h, cx + col * scale, cy + row * scale, scale, scale, r, g, b);
                }
            }
        }
        cx += 6 * scale;
    }
}

RGB lerp_rgb(RGB a, RGB b, float t) {
    return {
        static_cast<uint8_t>(a.r + (b.r - a.r) * t),
        static_cast<uint8_t>(a.g + (b.g - a.g) * t),
        static_cast<uint8_t>(a.b + (b.b - a.b) * t)
    };
}

RGB jet_color(float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    static const RGB stops[] = {
        {60,  110, 255},
        {40,  215, 255},
        {75,  235, 95},
        {250, 235, 45},
        {255, 55,  35}
    };
    const int n = 4;
    float pos = t * n;
    int idx = static_cast<int>(pos);
    if (idx >= n) return stops[n];
    return lerp_rgb(stops[idx], stops[idx + 1], pos - idx);
}

std::vector<KeyRect> build_100_layout() {
    const float r0 = 0.0f;
    const float r1 = 1.35f;
    const float r2 = 2.35f;
    const float r3 = 3.35f;
    const float r4 = 4.35f;
    const float r5 = 5.35f;
    const float nav = 15.35f;
    const float num = 18.70f;

    return {
        {0x01, 0.0f, r0, 1.0f, 1.0f, "ESC"},
        {0x3B, 2.0f, r0, 1.0f, 1.0f, "F1"}, {0x3C, 3.0f, r0, 1.0f, 1.0f, "F2"},
        {0x3D, 4.0f, r0, 1.0f, 1.0f, "F3"}, {0x3E, 5.0f, r0, 1.0f, 1.0f, "F4"},
        {0x3F, 6.5f, r0, 1.0f, 1.0f, "F5"}, {0x40, 7.5f, r0, 1.0f, 1.0f, "F6"},
        {0x41, 8.5f, r0, 1.0f, 1.0f, "F7"}, {0x42, 9.5f, r0, 1.0f, 1.0f, "F8"},
        {0x43, 11.0f, r0, 1.0f, 1.0f, "F9"}, {0x44, 12.0f, r0, 1.0f, 1.0f, "F10"},
        {0x57, 13.0f, r0, 1.0f, 1.0f, "F11"}, {0x58, 14.0f, r0, 1.0f, 1.0f, "F12"},
        {0x137, nav + 0.0f, r0, 1.0f, 1.0f, "PRT"}, {0x46, nav + 1.0f, r0, 1.0f, 1.0f, "SCR"}, {0x45, nav + 2.0f, r0, 1.0f, 1.0f, "PAU"},

        {0x29, 0.0f, r1, 1.0f, 1.0f, "`"}, {0x02, 1.0f, r1, 1.0f, 1.0f, "1"}, {0x03, 2.0f, r1, 1.0f, 1.0f, "2"},
        {0x04, 3.0f, r1, 1.0f, 1.0f, "3"}, {0x05, 4.0f, r1, 1.0f, 1.0f, "4"}, {0x06, 5.0f, r1, 1.0f, 1.0f, "5"},
        {0x07, 6.0f, r1, 1.0f, 1.0f, "6"}, {0x08, 7.0f, r1, 1.0f, 1.0f, "7"}, {0x09, 8.0f, r1, 1.0f, 1.0f, "8"},
        {0x0A, 9.0f, r1, 1.0f, 1.0f, "9"}, {0x0B, 10.0f, r1, 1.0f, 1.0f, "0"}, {0x0C, 11.0f, r1, 1.0f, 1.0f, "-"},
        {0x0D, 12.0f, r1, 1.0f, 1.0f, "="}, {0x0E, 13.0f, r1, 2.0f, 1.0f, "BACK"},
        {0x152, nav + 0.0f, r1, 1.0f, 1.0f, "INS"}, {0x147, nav + 1.0f, r1, 1.0f, 1.0f, "HOM"}, {0x149, nav + 2.0f, r1, 1.0f, 1.0f, "PGU"},
        {0x145, num + 0.0f, r1, 1.0f, 1.0f, "NUM"}, {0x135, num + 1.0f, r1, 1.0f, 1.0f, "/"}, {0x37, num + 2.0f, r1, 1.0f, 1.0f, "*"}, {0x4A, num + 3.0f, r1, 1.0f, 1.0f, "-"},

        {0x0F, 0.0f, r2, 1.5f, 1.0f, "TAB"}, {0x10, 1.5f, r2, 1.0f, 1.0f, "Q"}, {0x11, 2.5f, r2, 1.0f, 1.0f, "W"},
        {0x12, 3.5f, r2, 1.0f, 1.0f, "E"}, {0x13, 4.5f, r2, 1.0f, 1.0f, "R"}, {0x14, 5.5f, r2, 1.0f, 1.0f, "T"},
        {0x15, 6.5f, r2, 1.0f, 1.0f, "Y"}, {0x16, 7.5f, r2, 1.0f, 1.0f, "U"}, {0x17, 8.5f, r2, 1.0f, 1.0f, "I"},
        {0x18, 9.5f, r2, 1.0f, 1.0f, "O"}, {0x19, 10.5f, r2, 1.0f, 1.0f, "P"}, {0x1A, 11.5f, r2, 1.0f, 1.0f, "["},
        {0x1B, 12.5f, r2, 1.0f, 1.0f, "]"}, {0x2B, 13.5f, r2, 1.5f, 1.0f, "\\"},
        {0x153, nav + 0.0f, r2, 1.0f, 1.0f, "DEL"}, {0x14F, nav + 1.0f, r2, 1.0f, 1.0f, "END"}, {0x151, nav + 2.0f, r2, 1.0f, 1.0f, "PGD"},
        {0x47, num + 0.0f, r2, 1.0f, 1.0f, "7"}, {0x48, num + 1.0f, r2, 1.0f, 1.0f, "8"}, {0x49, num + 2.0f, r2, 1.0f, 1.0f, "9"}, {0x4E, num + 3.0f, r2, 1.0f, 2.0f, "+"},

        {0x3A, 0.0f, r3, 1.75f, 1.0f, "CAPS"}, {0x1E, 1.75f, r3, 1.0f, 1.0f, "A"}, {0x1F, 2.75f, r3, 1.0f, 1.0f, "S"},
        {0x20, 3.75f, r3, 1.0f, 1.0f, "D"}, {0x21, 4.75f, r3, 1.0f, 1.0f, "F"}, {0x22, 5.75f, r3, 1.0f, 1.0f, "G"},
        {0x23, 6.75f, r3, 1.0f, 1.0f, "H"}, {0x24, 7.75f, r3, 1.0f, 1.0f, "J"}, {0x25, 8.75f, r3, 1.0f, 1.0f, "K"},
        {0x26, 9.75f, r3, 1.0f, 1.0f, "L"}, {0x27, 10.75f, r3, 1.0f, 1.0f, ";"}, {0x28, 11.75f, r3, 1.0f, 1.0f, "'"},
        {0x1C, 12.75f, r3, 2.25f, 1.0f, "ENTER"},
        {0x4B, num + 0.0f, r3, 1.0f, 1.0f, "4"}, {0x4C, num + 1.0f, r3, 1.0f, 1.0f, "5"}, {0x4D, num + 2.0f, r3, 1.0f, 1.0f, "6"},

        {0x2A, 0.0f, r4, 2.25f, 1.0f, "SHIFT"}, {0x2C, 2.25f, r4, 1.0f, 1.0f, "Z"}, {0x2D, 3.25f, r4, 1.0f, 1.0f, "X"},
        {0x2E, 4.25f, r4, 1.0f, 1.0f, "C"}, {0x2F, 5.25f, r4, 1.0f, 1.0f, "V"}, {0x30, 6.25f, r4, 1.0f, 1.0f, "B"},
        {0x31, 7.25f, r4, 1.0f, 1.0f, "N"}, {0x32, 8.25f, r4, 1.0f, 1.0f, "M"}, {0x33, 9.25f, r4, 1.0f, 1.0f, ","},
        {0x34, 10.25f, r4, 1.0f, 1.0f, "."}, {0x35, 11.25f, r4, 1.0f, 1.0f, "/"}, {0x36, 12.25f, r4, 2.75f, 1.0f, "SHIFT"},
        {0x148, nav + 1.0f, r4, 1.0f, 1.0f, "^"},
        {0x4F, num + 0.0f, r4, 1.0f, 1.0f, "1"}, {0x50, num + 1.0f, r4, 1.0f, 1.0f, "2"}, {0x51, num + 2.0f, r4, 1.0f, 1.0f, "3"}, {0x11C, num + 3.0f, r4, 1.0f, 2.0f, "ENT"},

        {0x1D, 0.0f, r5, 1.25f, 1.0f, "CTRL"}, {0x15B, 1.25f, r5, 1.25f, 1.0f, "WIN"}, {0x38, 2.5f, r5, 1.25f, 1.0f, "ALT"},
        {0x39, 3.75f, r5, 6.25f, 1.0f, "SPACE"},
        {0x138, 10.0f, r5, 1.25f, 1.0f, "ALT"}, {0x15C, 11.25f, r5, 1.25f, 1.0f, "WIN"}, {0x15D, 12.5f, r5, 1.25f, 1.0f, "FN"}, {0x11D, 13.75f, r5, 1.25f, 1.0f, "CTRL"},
        {0x14B, nav + 0.0f, r5, 1.0f, 1.0f, "<"}, {0x150, nav + 1.0f, r5, 1.0f, 1.0f, "v"}, {0x14D, nav + 2.0f, r5, 1.0f, 1.0f, ">"},
        {0x52, num + 0.0f, r5, 2.0f, 1.0f, "0"}, {0x53, num + 2.0f, r5, 1.0f, 1.0f, "."}
    };
}

void render_heatmap(const std::array<std::atomic<uint32_t>, 512>& counts, const std::string& out_path) {
    const int unit = 68;
    const int gap = 7;
    const int case_pad = 24;
    const int outer_pad = 36;

    const int kb_w = static_cast<int>(22.7f * unit);
    const int kb_h = static_cast<int>(6.35f * unit);
    const int w = kb_w + (case_pad + outer_pad) * 2;
    const int h = kb_h + (case_pad + outer_pad) * 2;

    std::vector<uint8_t> img(w * h * 3);
    fill_rect(img, w, h, 0, 0, w, h, 255, 255, 255);

    int cx0 = outer_pad;
    int cy0 = outer_pad;
    int cw = kb_w + case_pad * 2;
    int ch = kb_h + case_pad * 2;

    fill_round_rect(img, w, h, cx0 + 2, cy0 + 5, cw, ch, 18, 210, 213, 219);
    for (int y = 0; y < ch; ++y) {
        float grad = static_cast<float>(y) / static_cast<float>(ch);
        uint8_t shade = static_cast<uint8_t>(224 - grad * 22);
        for (int x = 0; x < cw; ++x) {
            int dx = 0;
            int dy = 0;
            int rad = 16;
            if (x < rad) dx = rad - x;
            else if (x >= cw - rad) dx = x - (cw - rad - 1);
            if (y < rad) dy = rad - y;
            else if (y >= ch - rad) dy = y - (ch - rad - 1);
            if (dx * dx + dy * dy <= rad * rad) {
                put_pixel(img, w, h, cx0 + x, cy0 + y, shade, static_cast<uint8_t>(shade + 2), static_cast<uint8_t>(shade + 5));
            }
        }
    }
    stroke_round_rect(img, w, h, cx0, cy0, cw, ch, 16, 175, 179, 187);

    auto keys = build_100_layout();
    int ox = outer_pad + case_pad;
    int oy = outer_pad + case_pad;

    for (const auto& k : keys) {
        int x0 = ox + static_cast<int>(k.x * unit) + gap / 2;
        int y0 = oy + static_cast<int>(k.y * unit) + gap / 2;
        int rw = static_cast<int>(k.w * unit) - gap;
        int rh = static_cast<int>(k.h * unit) - gap;

        fill_round_rect(img, w, h, x0 + 1, y0 + 2, rw, rh, 8, 165, 169, 178);
        fill_round_rect(img, w, h, x0, y0, rw, rh, 8, 248, 249, 251);
        stroke_round_rect(img, w, h, x0, y0, rw, rh, 8, 188, 192, 200);
        draw_text_centered(img, w, h, x0, y0, rw, rh, 2, k.label, 125, 130, 140);
    }

    std::vector<float> heat(w * h, 0.0f);
    const float blob_r = unit * 1.15f;
    const float sigma = blob_r * 0.44f;
    const float inv_2s2 = 1.0f / (2.0f * sigma * sigma);
    const int rad_i = static_cast<int>(blob_r);

    for (const auto& k : keys) {
        uint32_t c = counts[k.sc].load();
        if (c == 0) continue;

        float weight = static_cast<float>(c);
        int cx = ox + static_cast<int>((k.x + k.w * 0.5f) * unit);
        int cy = oy + static_cast<int>((k.y + k.h * 0.5f) * unit);

        int y_start = std::max(0, cy - rad_i);
        int y_end   = std::min(h - 1, cy + rad_i);
        int x_start = std::max(0, cx - rad_i);
        int x_end   = std::min(w - 1, cx + rad_i);

        for (int y = y_start; y <= y_end; ++y) {
            float dy = static_cast<float>(y - cy);
            for (int x = x_start; x <= x_end; ++x) {
                float dx = static_cast<float>(x - cx);
                float d2 = dx * dx + dy * dy;
                if (d2 <= blob_r * blob_r) {
                    heat[y * w + x] += weight * std::exp(-d2 * inv_2s2);
                }
            }
        }
    }

    float max_heat = 0.0f;
    for (float v : heat) {
        if (v > max_heat) max_heat = v;
    }

    if (max_heat > 0.0f) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float v = heat[y * w + x];
                if (v <= 0.001f) continue;

                float t = std::pow(v / max_heat, 0.45f);
                if (t < 0.03f) continue;

                RGB col = jet_color(t);
                float alpha = std::min(0.82f, t * 1.15f);
                blend_pixel(img, w, h, x, y, col.r, col.g, col.b, alpha);
            }
        }
    }

    stbi_write_png(out_path.c_str(), w, h, 3, img.data(), w * 3);
}

struct State {
    std::array<std::atomic<uint32_t>, 512>* counts;
    std::atomic<bool> active{false};
};

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    State* st = reinterpret_cast<State*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (msg == WM_INPUT && st && st->active) {
        UINT size = 0;
        GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
        if (size > 0) {
            std::vector<uint8_t> buf(size);
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT, buf.data(), &size, sizeof(RAWINPUTHEADER)) == size) {
                RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buf.data());
                if (raw->header.dwType == RIM_TYPEKEYBOARD) {
                    const RAWKEYBOARD& kb = raw->data.keyboard;
                    if (!(kb.Flags & RI_KEY_BREAK) && kb.MakeCode != 0 && kb.MakeCode != 0xE0 && kb.MakeCode != 0xE1) {
                        uint16_t sc = kb.MakeCode & 0xFF;
                        if (kb.Flags & RI_KEY_E0) sc |= 0x100;
                        if (sc < 512) {
                            (*st->counts)[sc].fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                }
            }
        }
        return 0;
    }

    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

int main() {
    std::array<std::atomic<uint32_t>, 512> counts{};
    State st{&counts};
    std::atomic<DWORD> tid(0);

    std::thread worker([&]() {
        tid = GetCurrentThreadId();
        HINSTANCE inst = GetModuleHandle(nullptr);

        WNDCLASSA wc{};
        wc.lpfnWndProc = wnd_proc;
        wc.hInstance = inst;
        wc.lpszClassName = "raw_kb_sink";
        RegisterClassA(&wc);

        HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, inst, nullptr);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&st));

        RAWINPUTDEVICE rid{};
        rid.usUsagePage = 0x01;
        rid.usUsage = 0x06;
        rid.dwFlags = RIDEV_INPUTSINK;
        rid.hwndTarget = hwnd;
        RegisterRawInputDevices(&rid, 1, sizeof(rid));

        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    });

    std::cout << "keyboard-heatmap ready (/start, /stop, /exit)\n";
    std::string cmd;

    while (std::cin >> cmd) {
        if (cmd == "/start") {
            for (auto& c : counts) c.store(0);
            st.active = true;
            std::cout << "recording...\n";
        } else if (cmd == "/stop") {
            if (!st.active) {
                std::cout << "not recording right now\n";
                continue;
            }
            st.active = false;

            std::string dir = std::string(std::getenv("USERPROFILE")) + "\\Desktop\\";
            int n = 1;
            std::string out;
            do {
                out = dir + "keyboard_heatmap_" + std::to_string(n++) + ".png";
            } while (std::filesystem::exists(out));

            render_heatmap(counts, out);
            std::cout << "saved -> " << out << "\n";
        } else if (cmd == "/exit") {
            st.active = false;
            if (tid != 0) PostThreadMessage(tid, WM_QUIT, 0, 0);
            break;
        }
    }

    worker.join();
    return 0;
}