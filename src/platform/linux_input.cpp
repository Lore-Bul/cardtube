#include "linux_input.h"

#include "keyboard_map.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

#if !USE_DESKTOP
#include <cctype>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>
#endif

#ifndef APP_KEY_INPUT_DEVICE
#define APP_KEY_INPUT_DEVICE ""
#endif

namespace platform {
namespace {

std::array<lv_obj_t*, kNavKeyCount> nav_buttons{};
uint32_t last_key = 0;
bool last_key_pressed = false;
bool nav_shortcut_mode = false;
uint32_t pressed_key = 0;
uint32_t press_started_at = 0;
bool long_press_sent = false;
bool pressed_key_consumed = false;
lv_timer_t* long_press_timer = nullptr;
KeyReleaseListener key_release_listener = nullptr;
void* key_release_listener_user_data = nullptr;
GlobalKeyListener global_key_listener = nullptr;
void* global_key_listener_user_data = nullptr;

constexpr uint32_t kLongPressMs = 700;

#if !USE_DESKTOP
struct EvdevKeypad {
    int fd{-1};
    bool cardputer{false};
    lv_indev_state_t state{LV_INDEV_STATE_RELEASED};
    uint32_t key{0};
    bool router_event_pending{false};
    uint32_t router_key{0};
    bool router_pressed{false};
    bool router_repeat{false};
    // Remembers the character produced on press so that release and repeat
    // events keep it even if Shift changed in between.
    std::array<uint32_t, KEY_MAX + 1> mapped{};
};

std::vector<EvdevKeypad*> keypads;
bool shift_down = false;
#endif

size_t nav_key_to_index(uint32_t key) {
    if (!nav_shortcut_mode) {
        return kNavKeyCount;
    }

    switch (key) {
        case '4':
            return 0;
        case '5':
            return 1;
        case '6':
            return 2;
        case '7':
            return 3;
        case '8':
            return 4;
        default:
            return kNavKeyCount;
    }
}

void dispatch_nav_key(uint32_t key) {
    const auto index = nav_key_to_index(key);
    if (index >= nav_buttons.size()) {
        return;
    }

    auto* button = nav_buttons[index];
    if (!button || !lv_obj_is_valid(button) || !lv_obj_has_flag(button, LV_OBJ_FLAG_CLICKABLE)) {
        return;
    }

    lv_obj_send_event(button, LV_EVENT_CLICKED, nullptr);
}

bool emit_global_key(uint32_t key, bool long_pressed, bool repeat) {
    return global_key_listener &&
           global_key_listener(key, describe_key(key), long_pressed, repeat,
                               global_key_listener_user_data);
}

void long_press_timer_cb(lv_timer_t*) {
    if (!pressed_key || long_press_sent || lv_tick_elaps(press_started_at) < kLongPressMs) {
        return;
    }

    long_press_sent = true;
    if (emit_global_key(pressed_key, true, false)) {
        pressed_key_consumed = true;
    }
}

void ensure_long_press_timer() {
    if (!long_press_timer) {
        long_press_timer = lv_timer_create(long_press_timer_cb, 40, nullptr);
    }
    if (long_press_timer) {
        lv_timer_resume(long_press_timer);
        lv_timer_reset(long_press_timer);
    }
}

void route_key_state(uint32_t key, bool pressed, bool repeat) {
    if (repeat) {
        if (!long_press_sent) {
            emit_global_key(key, false, true);
        }
        last_key = key;
        last_key_pressed = true;
        return;
    }

    const bool new_hold = pressed && (!last_key_pressed || pressed_key != key);
    if (new_hold) {
        pressed_key = key;
        press_started_at = lv_tick_get();
        long_press_sent = false;
        pressed_key_consumed = emit_global_key(key, false, false);
        ensure_long_press_timer();
        if (!pressed_key_consumed) {
            dispatch_nav_key(key);
        }
    }
    else if (!pressed && last_key_pressed && pressed_key == key) {
        if (key_release_listener) {
            key_release_listener(key, describe_key(key), key_release_listener_user_data);
        }
        pressed_key = 0;
        pressed_key_consumed = false;
        if (long_press_timer) {
            lv_timer_pause(long_press_timer);
        }
    }

    last_key = key;
    last_key_pressed = pressed;
}

void key_event_cb(lv_event_t* event) {
    LV_UNUSED(event);

    auto* indev = lv_indev_active();
    if (!indev) {
        return;
    }

#if !USE_DESKTOP
    auto* keypad = static_cast<EvdevKeypad*>(lv_indev_get_driver_data(indev));
    if (keypad) {
        // LVGL emits LV_EVENT_KEY on every poll, even when evdev had no new event.
        if (!keypad->router_event_pending) {
            return;
        }

        keypad->router_event_pending = false;
        route_key_state(keypad->router_key, keypad->router_pressed, keypad->router_repeat);
        return;
    }
#endif

    const auto key = lv_indev_get_key(indev);
    const bool pressed = lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED;

    route_key_state(key, pressed, false);
}

#if !USE_DESKTOP
// The CardputerZero keyboard (tca8418c) has the kernel driver emit the already
// selected "Sym" layer as vendor keycodes that overlap with ordinary evdev codes
// (26 is "[" on a USB keyboard but "!" here), so the symbol table in
// keyboard_map.cpp must only be applied to that built-in keyboard.
bool is_cardputer_keyboard(const char* name) {
    if (!name || name[0] == '\0') {
        return false;
    }

    std::string lower;
    for (const char* cursor = name; *cursor != '\0'; ++cursor) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*cursor))));
    }
    return lower.find("tca8418") != std::string::npos;
}

bool next_event(EvdevKeypad& keypad, uint32_t& key, bool& pressed, bool& repeat) {
    input_event input{};
    while (read(keypad.fd, &input, sizeof(input)) == sizeof(input)) {
        if (input.type != EV_KEY) {
            continue;
        }
        if (input.code > KEY_MAX) {
            continue;
        }

        if (input.code == KEY_LEFTSHIFT || input.code == KEY_RIGHTSHIFT) {
            if (input.value == 0) {
                shift_down = false;
            }
            else if (input.value == 1) {
                shift_down = true;
            }
            continue;
        }

        // Caps lock is reported as 1/0 like a normal key; ignore it.
        if (input.code == KEY_CAPSLOCK) {
            continue;
        }

        uint32_t mapped = 0;
        if (input.value == 2) {
            // Repeats reuse the character selected when the key went down.
            mapped = keypad.mapped[input.code];
        }
        else {
            mapped = platform::map_evdev_key(input.code, shift_down, keypad.cardputer);
            if (input.value == 0) {
                const uint32_t pressed_code = keypad.mapped[input.code];
                keypad.mapped[input.code] = 0;
                mapped = pressed_code;
            }
            else {
                keypad.mapped[input.code] = mapped;
            }
        }
        if (!mapped) {
            continue;
        }

        key = mapped;
        pressed = input.value != 0;
        repeat = input.value == 2;
        return true;
    }
    return false;
}

bool has_nav_keys(int fd) {
    unsigned long key_bits[(KEY_MAX / (sizeof(unsigned long) * 8)) + 1] = {};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0) {
        return false;
    }

    auto has_key = [&](int code) {
        const auto bits_per_word = static_cast<int>(sizeof(unsigned long) * 8);
        return (key_bits[code / bits_per_word] & (1UL << (code % bits_per_word))) != 0;
    };

    return has_key(KEY_ESC) || has_key(KEY_LEFT) || has_key(KEY_RIGHT) || has_key(KEY_A) ||
           has_key(KEY_Q) || has_key(KEY_1) || has_key(KEY_ENTER) || has_key(KEY_SPACE) ||
           has_key(KEY_SYSRQ) || has_key(KEY_HELP);
}

void evdev_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    auto* keypad = static_cast<EvdevKeypad*>(lv_indev_get_driver_data(indev));
    data->continue_reading = false;
    if (!keypad) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    uint32_t key = 0;
    bool pressed = false;
    bool repeat = false;
    if (next_event(*keypad, key, pressed, repeat)) {
        keypad->key = key;
        keypad->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        keypad->router_event_pending = true;
        keypad->router_key = key;
        keypad->router_pressed = pressed;
        keypad->router_repeat = repeat;
        data->continue_reading = true;
    }

    data->key = keypad->key;
    data->state = keypad->state;
}

void evdev_delete_cb(lv_event_t* event) {
    auto* indev = static_cast<lv_event_t*>(event) ? static_cast<lv_indev_t*>(lv_event_get_target(event))
                                                   : nullptr;
    auto* keypad = indev ? static_cast<EvdevKeypad*>(lv_indev_get_driver_data(indev)) : nullptr;
    if (!keypad) {
        return;
    }

    for (auto it = keypads.begin(); it != keypads.end(); ++it) {
        if (*it == keypad) {
            keypads.erase(it);
            break;
        }
    }

    if (keypad->fd >= 0) {
        close(keypad->fd);
    }
    delete keypad;
}

lv_indev_t* create_keypad_from_fd(int fd, bool cardputer) {
    auto* keypad = new EvdevKeypad;
    keypad->fd = fd;
    keypad->cardputer = cardputer;

    auto* indev = lv_indev_create();
    if (!indev) {
        delete keypad;
        close(fd);
        return nullptr;
    }

    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev, evdev_read_cb);
    lv_indev_set_driver_data(indev, keypad);
    lv_indev_add_event_cb(indev, evdev_delete_cb, LV_EVENT_DELETE, nullptr);
    attach_key_router(indev);
    keypads.push_back(keypad);
    return indev;
}

lv_indev_t* try_create_keypad(const char* path) {
    if (!path || path[0] == '\0') {
        return nullptr;
    }

    const int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        LV_LOG_WARN("failed to open input device %s: %s", path, strerror(errno));
        return nullptr;
    }

    if (!has_nav_keys(fd)) {
        close(fd);
        return nullptr;
    }

    char name[256] = {};
    if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) < 0) {
        name[0] = '\0';
    }
    const bool cardputer = is_cardputer_keyboard(name);

    LV_LOG_INFO("using evdev key input %s (%s%s)",
                path,
                name[0] != '\0' ? name : "unknown",
                cardputer ? ", CardputerZero symbols" : "");
    return create_keypad_from_fd(fd, cardputer);
}

void discover_keypads(lv_display_t* display) {
    const char* configured_device = APP_KEY_INPUT_DEVICE;
    if (configured_device[0] != '\0') {
        auto* indev = try_create_keypad(configured_device);
        if (indev) {
            lv_indev_set_display(indev, display);
        }
        return;
    }

    auto* dir = opendir("/dev/input");
    if (!dir) {
        LV_LOG_WARN("failed to open /dev/input: %s", strerror(errno));
        return;
    }

    while (auto* entry = readdir(dir)) {
        if (std::strncmp(entry->d_name, "event", 5) != 0) {
            continue;
        }

        std::string path = "/dev/input/";
        path += entry->d_name;
        auto* indev = try_create_keypad(path.c_str());
        if (indev) {
            lv_indev_set_display(indev, display);
        }
    }

    closedir(dir);
}
#endif

} // namespace

void init_key_input(lv_display_t* display) {
#if !USE_DESKTOP
    discover_keypads(display);
#else
    LV_UNUSED(display);
#endif
}

void attach_key_router(lv_indev_t* indev) {
    if (!indev || lv_indev_get_type(indev) != LV_INDEV_TYPE_KEYPAD) {
        return;
    }

    lv_indev_add_event_cb(indev, key_event_cb, LV_EVENT_KEY, nullptr);
}

void set_nav_shortcut_mode(bool enabled) {
    nav_shortcut_mode = enabled;
}

void register_nav_button(size_t index, lv_obj_t* button) {
    if (index >= nav_buttons.size()) {
        return;
    }

    nav_buttons[index] = button;
}

void unregister_nav_button(size_t index, lv_obj_t* button) {
    if (index >= nav_buttons.size()) {
        return;
    }

    if (!button || nav_buttons[index] == button) {
        nav_buttons[index] = nullptr;
    }
}

void set_key_release_listener(KeyReleaseListener listener, void* user_data) {
    key_release_listener = listener;
    key_release_listener_user_data = user_data;
}

void clear_key_release_listener(KeyReleaseListener listener, void* user_data) {
    if (key_release_listener == listener && key_release_listener_user_data == user_data) {
        key_release_listener = nullptr;
        key_release_listener_user_data = nullptr;
    }
}

void set_global_key_listener(GlobalKeyListener listener, void* user_data) {
    global_key_listener = listener;
    global_key_listener_user_data = user_data;
}

void clear_global_key_listener(GlobalKeyListener listener, void* user_data) {
    if (global_key_listener == listener && global_key_listener_user_data == user_data) {
        global_key_listener = nullptr;
        global_key_listener_user_data = nullptr;
    }
}

void pump_input() {
#if !USE_DESKTOP
    for (auto* keypad : keypads) {
        if (!keypad || keypad->fd < 0) {
            continue;
        }
        uint32_t key = 0;
        bool pressed = false;
        bool repeat = false;
        while (next_event(*keypad, key, pressed, repeat)) {
            if (pressed) {
                emit_global_key(key, false, repeat);
            }
            else if (key_release_listener) {
                key_release_listener(key, describe_key(key), key_release_listener_user_data);
            }
        }
    }
#endif
}

const char* describe_key(uint32_t key) {
    switch (key) {
        case LV_KEY_ESC:
            return "Esc";
        case LV_KEY_ENTER:
            return "Enter";
        case LV_KEY_BACKSPACE:
            return "Backspace";
        case LV_KEY_NEXT:
            return "Tab";
        case LV_KEY_LEFT:
            return "Left";
        case LV_KEY_RIGHT:
            return "Right";
        case LV_KEY_UP:
            return "Up";
        case LV_KEY_DOWN:
            return "Down";
        case kKeyPrintScreen:
            return "PrintScreen";
        case kKeyHelp:
            return "Help";
        case kKeyMute:
            return "Mute";
        case kKeyVolumeDown:
            return "VolumeDown";
        case kKeyVolumeUp:
            return "VolumeUp";
        case kKeyPlayPause:
            return "PlayPause";
        case kKeyPreviousTrack:
            return "PreviousTrack";
        case kKeyNextTrack:
            return "NextTrack";
        case ' ':
            return "Space";
        default:
            break;
    }

    static char buffer[24];
    if (key >= 32 && key <= 126) {
        std::snprintf(buffer, sizeof(buffer), "%c", static_cast<char>(key));
    }
    else {
        std::snprintf(buffer, sizeof(buffer), "Key_%u", key);
    }
    return buffer;
}

} // namespace platform
