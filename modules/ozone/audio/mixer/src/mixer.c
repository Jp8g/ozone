#include <ozone/audio/mixer/mixer.h>
#include <ozone/audio/stream/stream.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint64_t oz_u48_16_fixed_to_uint64_t_floor(oz_u48_16_fixed input) {
	return input >> 16;
}

uint64_t oz_u48_16_fixed_to_uint64_t_round(oz_u48_16_fixed input) {
	return (input + 0x8000ULL) >> 16;
}

uint64_t oz_u48_16_fixed_to_uint64_t_ceil(oz_u48_16_fixed input) {
	return (input + 0xFFFFULL) >> 16;
}

float oz_u48_16_fixed_to_float(oz_u48_16_fixed input) {
	return (float)(input) / 65536.0f;
}

double oz_u48_16_fixed_to_double(oz_u48_16_fixed input) {
	return (double)(input) / 65536.0;
}

float oz_u48_16_fixed_get_fraction_float(oz_u48_16_fixed input) {
	return (float)(uint16_t)input / 65536.0f;
}

double oz_u48_16_fixed_get_fraction_double(oz_u48_16_fixed input) {
	return (double)(uint16_t)input / 65536.0;
}

oz_u48_16_fixed oz_uint64_t_to_u48_16_fixed(uint64_t input) {
	return input << 16;
}

oz_u48_16_fixed oz_float_to_u48_16_fixed(float input) {
	return input * 65536.0f;
}

oz_u48_16_fixed oz_double_to_u48_16_fixed(double input) {
	return input * 65536.0;
}

pthread_mutex_t instancesLock = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t oz_mixer_get_instances_mutex() {
	return instancesLock;
}

oz_audio_buffer* oz_create_audio_buffer(float* data, uint64_t frameCount, uint32_t sampleRate, uint32_t channels) {
	oz_audio_buffer* buffer = malloc(sizeof(oz_audio_buffer));
	if (!buffer) return NULL;

	buffer->data = data;
	buffer->frameCount = frameCount;
	buffer->sampleRate = sampleRate;
	buffer->channels = channels;

	return buffer;
}

oz_audio_buffer* oz_create_audio_buffer_copy(const float* data, uint64_t frameCount, uint32_t sampleRate, uint32_t channels) {
	oz_audio_buffer* buffer = malloc(sizeof(oz_audio_buffer));
	if (!buffer) return NULL;

	buffer->data = malloc(frameCount * channels * sizeof(float));
	if (!buffer->data) {
		free(buffer);
		return NULL;
	}

	memcpy(buffer->data, data, frameCount * channels * sizeof(float));

	buffer->frameCount = frameCount;
	buffer->sampleRate = sampleRate;
	buffer->channels = channels;

	return buffer;
}

oz_audio_instance* oz_create_audio_instance(oz_audio_buffer* buffer, oz_u48_16_fixed framePos, oz_u48_16_fixed speed, float volume, bool playing, bool loop) {
	oz_audio_instance* instance = malloc(sizeof(oz_audio_instance));
	if (!instance) return NULL;

	instance->buffer = buffer;
	instance->framePos = framePos;
	instance->speed = speed;
	instance->volume = volume;
	instance->playing = playing;
	instance->loop = loop;

	return instance;
}

oz_mixer* oz_create_mixer(uint32_t initialMixerCapacity) {
	oz_mixer* mixer = malloc(sizeof(oz_mixer));
	if (!mixer) return NULL;

	mixer->instances = malloc(initialMixerCapacity * sizeof(oz_audio_instance));
	mixer->instanceCapacity = initialMixerCapacity;
	mixer->instanceCount = 0;

	return mixer;
}

void oz_mixer_add_instance(oz_mixer* mixer, oz_audio_instance* instance) {
	pthread_mutex_lock(&instancesLock);
	mixer->instanceCount++;

	if (mixer->instanceCount > mixer->instanceCapacity) {
		mixer->instanceCapacity = mixer->instanceCapacity == 0 ? 1 : mixer->instanceCapacity * 2;
		mixer->instances = realloc(mixer->instances, mixer->instanceCapacity * sizeof(oz_audio_instance));
	}

	mixer->instances[mixer->instanceCount - 1] = instance;
	pthread_mutex_unlock(&instancesLock);
}

void oz_mixer_write_callback(float* outBuffer, oz_audio_stream* audioStream) {
	if (!audioStream->userData) return;
	oz_mixer mixer = *(oz_mixer*)audioStream->userData;

	pthread_mutex_lock(&instancesLock);
	for (uint32_t instanceIndex = 0; instanceIndex < mixer.instanceCount; instanceIndex++) {
		oz_audio_instance* instance = mixer.instances[instanceIndex];

		if (!instance->playing) continue;

		oz_audio_buffer buffer = *instance->buffer;
		
		for (uint64_t frame = 0; frame < audioStream->periodSize; frame++) {
			if (oz_u48_16_fixed_to_uint64_t_ceil(instance->framePos) > buffer.frameCount) {
				instance->framePos = 0;
				if (!instance->loop) {
					instance->playing = false;
					break;
				}			
			}

			for (uint32_t channel = 0; channel < audioStream->channels; channel++) {
				float sample = 0;

				if (buffer.channels == audioStream->channels) {
					float fraction = oz_u48_16_fixed_get_fraction_float(instance->framePos);
					float f1 = buffer.data[oz_u48_16_fixed_to_uint64_t_floor(instance->framePos) * buffer.channels + channel];
					float f2 = buffer.data[oz_u48_16_fixed_to_uint64_t_ceil(instance->framePos) * buffer.channels + channel];
					sample = (f1 * (1 - fraction)) + (f2 * fraction);
				} else if (buffer.channels == 1 && audioStream->channels == 2) {
					float fraction = oz_u48_16_fixed_get_fraction_float(instance->framePos);
					float f1 = buffer.data[oz_u48_16_fixed_to_uint64_t_floor(instance->framePos)];
					float f2 = buffer.data[oz_u48_16_fixed_to_uint64_t_ceil(instance->framePos)];
					sample = (f1 * (1 - fraction)) + (f2 * fraction);
				} else if (buffer.channels == 2 && audioStream->channels == 1) {
					float fraction = oz_u48_16_fixed_get_fraction_float(instance->framePos);
					uint64_t framePosFloor = oz_u48_16_fixed_to_uint64_t_floor(instance->framePos);
					uint64_t framePosCeil = oz_u48_16_fixed_to_uint64_t_ceil(instance->framePos);

					float f1 = buffer.data[framePosFloor * buffer.channels] + buffer.data[framePosFloor * buffer.channels + 1];
					float f2 = buffer.data[framePosCeil * buffer.channels] + buffer.data[framePosCeil * buffer.channels + 1];
					sample = (f1 * (1 - fraction)) + (f2 * fraction);
				} else {
					printf("ts pmo\n");
				}

				if (outBuffer) outBuffer[frame * audioStream->channels + channel] += sample * instance->volume;
			}

			instance->framePos += oz_double_to_u48_16_fixed((double)buffer.sampleRate * oz_u48_16_fixed_to_double(instance->speed)) / audioStream->sampleRate;
		}
	}
	pthread_mutex_unlock(&instancesLock);
}