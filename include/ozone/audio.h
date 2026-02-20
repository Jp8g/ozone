#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
#include <atomic>
#else
#include <stdatomic.h>
#endif

/* CORE */
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

/* MIXER */
typedef uint64_t oz_u48_16_fixed;
typedef uint64_t oz_i48_16_fixed;

typedef struct oz_audio_buffer {
    float* data;
    uint64_t frameCount;
    uint32_t sampleRate;
    uint32_t channels;
} oz_audio_buffer;

typedef struct oz_audio_instance {
    oz_audio_buffer* buffer;
    oz_u48_16_fixed framePos;
    oz_u48_16_fixed speed;
    float volume;
    bool playing;
    bool loop;
} oz_audio_instance;

typedef struct oz_mixer {
    oz_audio_instance** instances;
    uint32_t instanceCapacity;
    uint32_t instanceCount;
} oz_mixer;

uint64_t oz_u48_16_fixed_to_uint64_t_floor(oz_u48_16_fixed input);
uint64_t oz_u48_16_fixed_to_uint64_t_round(oz_u48_16_fixed input);
uint64_t oz_u48_16_fixed_to_uint64_t_ceil(oz_u48_16_fixed input);
float oz_u48_16_fixed_to_float(oz_u48_16_fixed input);
double oz_u48_16_fixed_to_double(oz_u48_16_fixed input);
float oz_u48_16_fixed_get_fraction_float(oz_u48_16_fixed input);
double oz_u48_16_fixed_get_fraction_double(oz_u48_16_fixed input);
oz_u48_16_fixed oz_uint64_t_to_u48_16_fixed(uint64_t input);
oz_u48_16_fixed oz_float_to_u48_16_fixed(float input);
oz_u48_16_fixed oz_double_to_u48_16_fixed(double input);

oz_audio_buffer* oz_create_audio_buffer(float* data, uint64_t frameCount, uint32_t sampleRate, uint32_t channels);
oz_audio_buffer* oz_create_audio_buffer_copy(const float* data, uint64_t frameCount, uint32_t sampleRate, uint32_t channels);
oz_audio_instance* oz_create_audio_instance(oz_audio_buffer* buffer, oz_u48_16_fixed framePos, oz_u48_16_fixed speed, float volume, bool playing, bool loop);
oz_mixer* oz_create_mixer(uint32_t initialMixerCapacity);
void oz_mixer_add_instance(oz_mixer* mixer, oz_audio_instance* instance);
void oz_mixer_write_callback(float* outBuffer, oz_audio_stream* stream);