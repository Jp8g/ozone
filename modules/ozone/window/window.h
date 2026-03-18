#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

typedef uint32_t oz_window;

typedef void oz_wl_display;
typedef void oz_wl_surface;

typedef enum oz_graphics_backend {
	OZ_GRAPHICS_BACKEND_NONE,
	OZ_GRAPHICS_BACKEND_SOFTWARE,
	OZ_GRAPHICS_BACKEND_OPENGL,
	OZ_GRAPHICS_BACKEND_VULKAN,
} oz_graphics_backend;

typedef enum oz_key {
    OZ_KEY_RESERVED = 0,
    OZ_KEY_ESC = 1,
    OZ_KEY_1 = 2,
    OZ_KEY_2 = 3,
    OZ_KEY_3 = 4,
    OZ_KEY_4 = 5,
    OZ_KEY_5 = 6,
    OZ_KEY_6 = 7,
    OZ_KEY_7 = 8,
    OZ_KEY_8 = 9,
    OZ_KEY_9 = 10,
    OZ_KEY_0 = 11,
    OZ_KEY_MINUS = 12,
    OZ_KEY_EQUAL = 13,
    OZ_KEY_BACKSPACE = 14,
    OZ_KEY_TAB = 15,
    OZ_KEY_Q = 16,
    OZ_KEY_W = 17,
    OZ_KEY_E = 18,
    OZ_KEY_R = 19,
    OZ_KEY_T = 20,
    OZ_KEY_Y = 21,
    OZ_KEY_U = 22,
    OZ_KEY_I = 23,
    OZ_KEY_O = 24,
    OZ_KEY_P = 25,
    OZ_KEY_LEFTBRACE = 26,
    OZ_KEY_RIGHTBRACE = 27,
    OZ_KEY_ENTER = 28,
    OZ_KEY_LEFTCTRL = 29,
    OZ_KEY_A = 30,
    OZ_KEY_S = 31,
    OZ_KEY_D = 32,
    OZ_KEY_F = 33,
    OZ_KEY_G = 34,
    OZ_KEY_H = 35,
    OZ_KEY_J = 36,
    OZ_KEY_K = 37,
    OZ_KEY_L = 38,
    OZ_KEY_SEMICOLON = 39,
    OZ_KEY_APOSTROPHE = 40,
    OZ_KEY_GRAVE = 41,
    OZ_KEY_LEFTSHIFT = 42,
    OZ_KEY_BACKSLASH = 43,
    OZ_KEY_Z = 44,
    OZ_KEY_X = 45,
    OZ_KEY_C = 46,
    OZ_KEY_V = 47,
    OZ_KEY_B = 48,
    OZ_KEY_N = 49,
    OZ_KEY_M = 50,
    OZ_KEY_COMMA = 51,
    OZ_KEY_DOT = 52,
    OZ_KEY_SLASH = 53,
    OZ_KEY_RIGHTSHIFT = 54,
    OZ_KEY_KPASTERISK = 55,
    OZ_KEY_LEFTALT = 56,
    OZ_KEY_SPACE = 57,
    OZ_KEY_CAPSLOCK = 58,
    OZ_KEY_F1 = 59,
    OZ_KEY_F2 = 60,
    OZ_KEY_F3 = 61,
    OZ_KEY_F4 = 62,
    OZ_KEY_F5 = 63,
    OZ_KEY_F6 = 64,
    OZ_KEY_F7 = 65,
    OZ_KEY_F8 = 66,
    OZ_KEY_F9 = 67,
    OZ_KEY_F10 = 68,
    OZ_KEY_NUMLOCK = 69,
    OZ_KEY_SCROLLLOCK = 70,
    OZ_KEY_KP7 = 71,
    OZ_KEY_KP8 = 72,
    OZ_KEY_KP9 = 73,
    OZ_KEY_KPMINUS = 74,
    OZ_KEY_KP4 = 75,
    OZ_KEY_KP5 = 76,
    OZ_KEY_KP6 = 77,
    OZ_KEY_KPPLUS = 78,
    OZ_KEY_KP1 = 79,
    OZ_KEY_KP2 = 80,
    OZ_KEY_KP3 = 81,
    OZ_KEY_KP0 = 82,
    OZ_KEY_KPDOT = 83,

    OZ_KEY_F11 = 87,
    OZ_KEY_F12 = 88,

    OZ_KEY_KPENTER = 96,
    OZ_KEY_RIGHTCTRL = 97,
    OZ_KEY_KPSLASH = 98,
    OZ_KEY_PRINTSCREEN = 99,
    OZ_KEY_RIGHTALT = 100,

    OZ_KEY_HOME = 102,
    OZ_KEY_UP = 103,
    OZ_KEY_PAGEUP = 104,
    OZ_KEY_LEFT = 105,
    OZ_KEY_RIGHT = 106,
    OZ_KEY_END = 107,
    OZ_KEY_DOWN = 108,
    OZ_KEY_PAGEDOWN = 109,
    OZ_KEY_INSERT = 110,
    OZ_KEY_DELETE = 111,

    OZ_KEY_LEFTMETA = 125,
    OZ_KEY_RIGHTMETA = 126,
} oz_key;

void oz_window_system_init();
oz_window oz_window_create(uint32_t width, uint32_t height, char* title, oz_graphics_backend graphics_backend, bool vsync);
uint8_t* oz_window_get_pixels(oz_window window);
oz_wl_display* oz_window_get_wayland_display();
oz_wl_display* oz_window_get_wayland_surface(oz_window window);
uint32_t oz_window_get_width(oz_window window);
uint32_t oz_window_get_height(oz_window window);
double oz_window_pointer_get_x(oz_window window);
double oz_window_pointer_get_y(oz_window window);
bool oz_window_is_open(oz_window window);
void oz_window_poll_events(oz_window window);
bool oz_window_is_key_down(oz_window window, oz_key key);
bool oz_window_frame_can_render(oz_window window);
void oz_window_display_frame(oz_window window);
void oz_window_close(oz_window window);
void oz_window_system_shutdown();