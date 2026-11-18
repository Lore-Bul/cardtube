/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "keyboard_map.h"

#include "linux_input.h"

#include "lvgl.h"

#include <array>
#include <cstring>

namespace platform {
namespace {

// Linux input-event-codes.h values, repeated here so the decoder also builds on
// the desktop host where the header does not exist.
namespace evdev {
constexpr uint16_t kEsc = 1;
constexpr uint16_t kDigit1 = 2;
constexpr uint16_t kDigit0 = 11;
constexpr uint16_t kMinus = 12;
constexpr uint16_t kEqual = 13;
constexpr uint16_t kBackspace = 14;
constexpr uint16_t kTab = 15;
constexpr uint16_t kQ = 16;
constexpr uint16_t kP = 25;
constexpr uint16_t kLeftBrace = 26;
constexpr uint16_t kRightBrace = 27;
constexpr uint16_t kEnter = 28;
constexpr uint16_t kA = 30;
constexpr uint16_t kL = 38;
constexpr uint16_t kSemicolon = 39;
constexpr uint16_t kApostrophe = 40;
constexpr uint16_t kGrave = 41;
constexpr uint16_t kBackslash = 43;
constexpr uint16_t kZ = 44;
constexpr uint16_t kM = 50;
constexpr uint16_t kComma = 51;
constexpr uint16_t kDot = 52;
constexpr uint16_t kSlash = 53;
constexpr uint16_t kSpace = 57;
constexpr uint16_t k102nd = 86;
constexpr uint16_t kKpEnter = 96;
constexpr uint16_t kSysRq = 99;
constexpr uint16_t kHome = 102;
constexpr uint16_t kUp = 103;
constexpr uint16_t kPageUp = 104;
constexpr uint16_t kLeft = 105;
constexpr uint16_t kRight = 106;
constexpr uint16_t kEnd = 107;
constexpr uint16_t kDown = 108;
constexpr uint16_t kPageDown = 109;
constexpr uint16_t kInsert = 110;
constexpr uint16_t kDelete = 111;
constexpr uint16_t kMute = 113;
constexpr uint16_t kVolumeDown = 114;
constexpr uint16_t kVolumeUp = 115;
constexpr uint16_t kHelp = 138;
constexpr uint16_t kNextSong = 163;
constexpr uint16_t kPlayPause = 164;
constexpr uint16_t kPreviousSong = 165;
constexpr uint16_t kBrightnessDown = 224;
constexpr uint16_t kBrightnessUp = 225;
} // namespace evdev

struct Symbol {
    uint16_t code;
    char character;
};

// Symbol layer of the built-in CardputerZero keyboard, as published by M5Stack
// (launcher cp0_keyboard_keymap.c and the installed
// /usr/share/keymaps/tca8418_keypad_m5stack_keymap.map).
constexpr std::array<Symbol, 32> kCardputerSymbols{{
    {26, '!'},  {27, '@'},  {39, '#'},  {40, '$'},  {41, '%'},  {43, '^'},
    {51, '&'},  {52, '*'},  {53, '('},  {94, ')'},  {55, '~'},  {69, '`'},
    {70, '_'},  {71, '-'},  {72, '+'},  {73, '='},  {74, '['},  {75, ']'},
    {76, '{'},  {77, '}'},  {79, ';'},  {80, ':'},  {81, '\''}, {82, '"'},
    {83, '<'},  {85, '>'},  {86, '\\'}, {89, '|'},  {90, ','},  {91, '.'},
    {92, '/'},  {93, '?'},
}};

uint32_t character_of(char value) {
    return static_cast<uint32_t>(static_cast<unsigned char>(value));
}

uint32_t letter_of(uint16_t code, uint16_t first, char base, bool shift) {
    const uint32_t offset = code - first;
    if (offset > 25) {
        return 0;
    }
    char character = static_cast<char>(base + offset);
    if (shift && character >= 'a' && character <= 'z') {
        character = static_cast<char>(character - 'a' + 'A');
    }
    return character_of(character);
}

uint32_t cardputer_symbol(uint16_t code) {
    for (const auto& symbol : kCardputerSymbols) {
        if (symbol.code == code) {
            return character_of(symbol.character);
        }
    }
    return 0;
}

} // namespace

uint32_t map_evdev_key(uint16_t code, bool shift, bool cardputer_keyboard) {
    if (cardputer_keyboard) {
        // The driver already selected the Sym layer: do not require a physical
        // Sym-down state, it also covers one-shot, locked and pre-launch modes.
        const auto symbol = cardputer_symbol(code);
        if (symbol) {
            return symbol;
        }
    }

    if (code >= evdev::kQ && code <= evdev::kP) {
        return letter_of(code, evdev::kQ, 'q', shift);
    }
    if (code >= evdev::kA && code <= evdev::kL) {
        return letter_of(code, evdev::kA, 'a', shift);
    }
    if (code >= evdev::kZ && code <= evdev::kM) {
        return letter_of(code, evdev::kZ, 'z', shift);
    }
    if (code >= evdev::kDigit1 && code <= evdev::kDigit0) {
        constexpr char digits[] = "1234567890";
        constexpr char symbols[] = "!@#$%^&*()";
        const auto index = static_cast<std::size_t>(code - evdev::kDigit1);
        return character_of(shift ? symbols[index] : digits[index]);
    }

    switch (code) {
        case evdev::kEsc:
            return LV_KEY_ESC;
        case evdev::kLeft:
            return LV_KEY_LEFT;
        case evdev::kRight:
            return LV_KEY_RIGHT;
        case evdev::kUp:
            return LV_KEY_UP;
        case evdev::kDown:
            return LV_KEY_DOWN;
        case evdev::kHome:
            return LV_KEY_HOME;
        case evdev::kEnd:
            return LV_KEY_END;
        case evdev::kDelete:
            return LV_KEY_DEL;
        case evdev::kEnter:
        case evdev::kKpEnter:
            return LV_KEY_ENTER;
        case evdev::kBackspace:
            return LV_KEY_BACKSPACE;
        case evdev::kTab:
            return shift ? LV_KEY_PREV : LV_KEY_NEXT;
        case evdev::kSpace:
            return ' ';
        case evdev::kMinus:
            return character_of(shift ? '_' : '-');
        case evdev::kEqual:
            return character_of(shift ? '+' : '=');
        case evdev::kLeftBrace:
            return character_of(shift ? '{' : '[');
        case evdev::kRightBrace:
            return character_of(shift ? '}' : ']');
        case evdev::kSemicolon:
            return character_of(shift ? ':' : ';');
        case evdev::kApostrophe:
            return character_of(shift ? '"' : '\'');
        case evdev::kGrave:
            return character_of(shift ? '~' : '`');
        case evdev::kBackslash:
            return character_of(shift ? '|' : '\\');
        case evdev::k102nd:
            return character_of(shift ? '>' : '<');
        case evdev::kComma:
            return character_of(shift ? '<' : ',');
        case evdev::kDot:
            return character_of(shift ? '>' : '.');
        case evdev::kSlash:
            return character_of(shift ? '?' : '/');
        case evdev::kMute:
            return kKeyMute;
        case evdev::kVolumeDown:
            return kKeyVolumeDown;
        case evdev::kVolumeUp:
            return kKeyVolumeUp;
        case evdev::kPlayPause:
            return kKeyPlayPause;
        case evdev::kPreviousSong:
            return kKeyPreviousTrack;
        case evdev::kNextSong:
            return kKeyNextTrack;
        case evdev::kPageUp:
        case evdev::kPageDown:
        case evdev::kInsert:
        case evdev::kBrightnessDown:
        case evdev::kBrightnessUp:
            return 0; // Fn-layer actions handled by the desktop, never text.
        case evdev::kSysRq:
            return kKeyPrintScreen;
        case evdev::kHelp:
            return kKeyHelp;
        default:
            return 0;
    }
}

bool keyboard_map_self_test(std::string& error) {
    struct Case {
        const char* name;
        uint16_t code;
        bool shift;
        bool cardputer;
        uint32_t expected;
    };

    static constexpr Case cases[] = {
        {"cardputer ! (code 26)", 26, false, true, '!'},
        {"cardputer @ (code 27)", 27, false, true, '@'},
        {"cardputer # (code 39)", 39, false, true, '#'},
        {"cardputer ^ (code 43)", 43, false, true, '^'},
        {"cardputer ( (code 53)", 53, false, true, '('},
        {"cardputer ) (code 94)", 94, false, true, ')'},
        {"cardputer ~ (code 55)", 55, false, true, '~'},
        {"cardputer _ (code 70)", 70, false, true, '_'},
        {"cardputer [ (code 74)", 74, false, true, '['},
        {"cardputer { (code 76)", 76, false, true, '{'},
        {"cardputer : (code 80)", 80, false, true, ':'},
        {"cardputer ' (code 81)", 81, false, true, '\''},
        {"cardputer \" (code 82)", 82, false, true, '"'},
        {"cardputer backslash (code 86)", 86, false, true, '\\'},
        {"cardputer | (code 89)", 89, false, true, '|'},
        {"cardputer ? (code 93)", 93, false, true, '?'},
        {"usb [ (code 26)", 26, false, false, '['},
        {"usb { (code 26, shift)", 26, true, false, '{'},
        {"usb ? (code 53, shift)", 53, true, false, '?'},
        {"digit 1", 2, false, true, '1'},
        {"shift+1", 2, true, true, '!'},
        {"shift+2", 3, true, true, '@'},
        {"letter a", 30, false, true, 'a'},
        {"shift+letter a", 30, true, true, 'A'},
        {"enter", 28, false, true, LV_KEY_ENTER},
        {"backspace", 14, false, true, LV_KEY_BACKSPACE},
        {"space", 57, false, true, ' '},
        {"fn+f up", 103, false, true, LV_KEY_UP},
        {"fn+c right", 106, false, true, LV_KEY_RIGHT},
        {"fn+backspace delete", 111, false, true, LV_KEY_DEL},
        {"fn+j printscreen", 99, false, true, kKeyPrintScreen},
        {"fn+h help", 138, false, true, kKeyHelp},
        {"fn+q play/pause", 164, false, true, kKeyPlayPause},
        {"fn+w previous", 165, false, true, kKeyPreviousTrack},
        {"fn+e next", 163, false, true, kKeyNextTrack},
        {"fn+a mute", 113, false, true, kKeyMute},
        {"fn+s volume down", 114, false, true, kKeyVolumeDown},
        {"fn+d volume up", 115, false, true, kKeyVolumeUp},
        {"fn+u brightness is not text", 224, false, true, 0},
        {"unknown key", 250, false, true, 0},
    };

    for (const auto& test : cases) {
        const auto actual =
            map_evdev_key(test.code, test.shift, test.cardputer);
        if (actual != test.expected) {
            error = std::string(test.name) + ": expected " +
                    std::to_string(test.expected) + ", got " +
                    std::to_string(actual);
            return false;
        }
    }

    error.clear();
    return true;
}

} // namespace platform
