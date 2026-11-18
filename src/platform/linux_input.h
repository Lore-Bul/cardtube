#pragma once

#include "lvgl.h"

#include <cstddef>
#include <cstdint>

namespace platform {

enum class KeyboardLayout {
    Us,
    It,
};

constexpr size_t kNavKeyCount = 5;
constexpr uint32_t kSpecialKeyBase = 0x200000U;
constexpr uint32_t kKeyPrintScreen = kSpecialKeyBase | 18U;
constexpr uint32_t kKeyHelp = kSpecialKeyBase | 19U;

using KeyReleaseListener = void (*)(uint32_t key, const char* key_name, void* user_data);
using GlobalKeyListener = bool (*)(uint32_t key,
                                   const char* key_name,
                                   bool long_pressed,
                                   bool repeat,
                                   void* user_data);

void init_key_input(lv_display_t* display);
void attach_key_router(lv_indev_t* indev);

// When enabled, number keys 4..8 activate the matching nav bar page. Disable it
// on pages where the keyboard is used for text entry.
void set_nav_shortcut_mode(bool enabled);
void register_nav_button(size_t index, lv_obj_t* button);
void unregister_nav_button(size_t index, lv_obj_t* button);

// Selects the character layout used by the evdev keyboard (US or Italian).
void set_keyboard_layout(KeyboardLayout layout);
KeyboardLayout keyboard_layout();
void set_key_release_listener(KeyReleaseListener listener, void* user_data);
void clear_key_release_listener(KeyReleaseListener listener, void* user_data);
void set_global_key_listener(GlobalKeyListener listener, void* user_data);
void clear_global_key_listener(GlobalKeyListener listener, void* user_data);
const char* describe_key(uint32_t key);

// Reads pending evdev events without going through LVGL's refresh loop, so the
// UI can stay frozen while mpv draws straight to the framebuffer. Only global
// key listeners are notified.
void pump_input();

} // namespace platform
