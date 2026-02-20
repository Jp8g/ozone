#include <stdint.h>

typedef uint32_t oz_window;
typedef struct oz_gfx_context oz_gfx_context;

oz_gfx_context* oz_graphics_system_init(oz_window window);
void oz_render_frame(oz_gfx_context* gfxContext);
void oz_graphics_system_shutdown(oz_gfx_context* gfxContext);