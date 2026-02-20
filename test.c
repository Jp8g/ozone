#include <bits/time.h>
#include <ozone/window.h>
#include <ozone/audio.h>
#include <ozone/graphics.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

struct timespec start, end;
bool timer_init = false;

void timer(const char* input) {
    if (!timer_init) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        timer_init = true;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    printf("%s: %fms\n", input, (float)(end.tv_sec - start.tv_sec) * 1000.0f + (float)(end.tv_nsec - start.tv_nsec) / 1000000.0f);

    start = end;
}

int main() {
    struct timespec totalStart, totalEnd;
    clock_gettime(CLOCK_MONOTONIC, &totalStart);

    timer("start");
    oz_window_system_init();
    printf("initialised window system\n");
    oz_window window = oz_create_window(800, 600, "Ozone Test", OZ_GRAPHICS_BACKEND_VULKAN, true);
    timer("created window");

    oz_mixer* mixer = oz_create_mixer(1);
    timer("created mixer");
    oz_audio_stream* stream = oz_create_audio_stream(oz_mixer_write_callback, 2, 48000, 256, 128, mixer);
    timer("created audio stream");

    drwav file;

    if (!drwav_init_file(&file, "trapezium.wav", NULL)) return 1;

    float* data = malloc(file.totalPCMFrameCount * file.channels * sizeof(float));
    if (!data) {
        drwav_uninit(&file);
        return 1;
    }

    oz_audio_buffer* buffer = oz_create_audio_buffer(data, file.totalPCMFrameCount, file.sampleRate, file.channels);
    if (!buffer) {
        drwav_uninit(&file);
        return 1;
    }

    drwav_read_pcm_frames_f32(&file, file.totalPCMFrameCount, data);
    drwav_uninit(&file);
    timer("read wav file");

    oz_audio_instance* instance = oz_create_audio_instance(buffer, 0, oz_float_to_u48_16_fixed(1), 1, true, true);
    timer("created audio instance");
    oz_mixer_add_instance(mixer, instance);
    timer("added audio instance");


    oz_gfx_context* gfxContext = oz_graphics_system_init(window);
    timer("initialised graphics system");

    bool first_frame = true;

    while (oz_window_is_open(window)) {
        oz_poll_events(window);

        if (oz_frame_can_render(window)) {
            if (oz_is_key_down(window, OZ_KEY_ESC)) {
                break;
            }

            oz_render_frame(gfxContext);

            oz_display_frame(window);

            if (first_frame) {
                first_frame = false;
                clock_gettime(CLOCK_MONOTONIC, &totalEnd);
                float diff = (float)(totalEnd.tv_sec - totalStart.tv_sec) * 1000.0f + (float)(totalEnd.tv_nsec - totalStart.tv_nsec) / 1000000.0f;
                printf("Total time: %f\n", diff);
            }
        }
    }

    printf("shutting down\n");
    oz_close_window(window);
    oz_close_audio_stream(stream);
    free(buffer->data);
    free(buffer);
    free(instance);
    free(mixer->instances);
    free(mixer);
    oz_graphics_system_shutdown(gfxContext);
    oz_window_system_shutdown();
    return 0;
}