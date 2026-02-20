#include <ozone/audio.h>
#include <dryad/dryad.h>

oz_audio_stream* oz_create_audio_stream(oz_write_callback writeCallback, uint32_t channels, uint32_t sampleRate, uint64_t bufferSize, uint64_t periodSize, void* userData) {
	return (oz_audio_stream*)dry_create_audio_stream((dry_write_callback)writeCallback, channels, sampleRate, bufferSize, periodSize, userData);
}

void oz_close_audio_stream(oz_audio_stream* audioStream) {
	dry_close_audio_stream((dry_audio_stream*)audioStream);
}