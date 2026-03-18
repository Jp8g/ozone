#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
#include <atomic>
#else
#include <stdatomic.h>
#endif

typedef struct oz_audio_stream oz_audio_stream;
typedef void (*oz_write_callback)(float*, oz_audio_stream*);

struct oz_audio_stream {
    void* internal;
    
    pthread_t thread;
    oz_write_callback writeCallback;
    void* userData;

    uint64_t bufferSize;
    uint64_t periodSize;
    uint32_t sampleRate;
    uint32_t channels;

    #ifdef __cplusplus
    std::atomic<bool> active;
    #else
    _Atomic bool active;
    #endif
};

oz_audio_stream* oz_create_audio_stream(oz_write_callback writeCallback, uint32_t channels, uint32_t sampleRate, uint64_t bufferSize, uint64_t periodSize, void* userData);
void oz_close_audio_stream(oz_audio_stream* audioStream);