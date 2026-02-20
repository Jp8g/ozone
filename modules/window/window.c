#include <ozone/window.h>
#include <kipcorn/kipcorn.h>

void oz_window_system_init() {
	kip_init();
}

oz_window oz_create_window(uint32_t width, uint32_t height, char* title, oz_graphics_backend graphics_backend, bool vsync) {
	return kip_create_window(width, height, title, (kip_graphics_backend)graphics_backend, vsync, true, false, NULL);
}

oz_wl_display* oz_get_wayland_display() {
	return kip_get_wayland_display();
}

oz_wl_display* oz_get_wayland_surface(oz_window window) {
	return kip_get_wayland_surface(window);
}

uint32_t oz_window_get_width(oz_window window) {
	return kip_get_width(window);
}

uint32_t oz_window_get_height(oz_window window) {
	return kip_get_height(window);
}

bool oz_window_is_open(oz_window window) {
	return kip_window_is_open(window);
}

void oz_poll_events(oz_window window) {
	kip_poll_events(kip_get_vsync(window));
}

bool oz_is_key_down(oz_window window, oz_key key) {
	return kip_is_key_down(window, (kip_key)key);
}

bool oz_frame_can_render(oz_window window) {
	return kip_frame_can_render(window);
}

void oz_display_frame(oz_window window) {
	kip_submit_frame(window);
}

void oz_close_window(oz_window window) {
	kip_close_window(window);
}

void oz_window_system_shutdown() {
	kip_shutdown();
}