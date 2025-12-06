#ifndef AUDIO_H
#define AUDIO_H

#include <glib.h>
#include <stdint.h>

typedef struct {
  uint32_t index;      // PulseAudio sink index
  char *name;          // Friendly name (e.g., "Built-in Audio Analog Stereo")
  int volume;          // 0-100 (average of channels)
  int is_default;      // 1 if this is the default sink, 0 otherwise
} AudioDevice;

typedef struct {
  int pid;             // Process ID
  uint32_t sink_index; // Sink index this stream is playing to
  uint32_t stream_index; // Sink input index (to identify stream for killing)
} StreamInfo;

// Kill a specific audio stream (sink input)
void kill_audio_stream(uint32_t stream_index);

// Move a specific audio stream to a new sink (device)
void move_audio_stream(uint32_t stream_index, uint32_t sink_index);

// Get list of audio sinks (output devices)
GList* get_audio_devices(void);

// Set volume for a device (0-100)
void set_device_volume(int device_index, int volume);

// Get list of streams (PIDs) that are currently playing audio
// Returns a GList of StreamInfo pointers (must free with g_list_free_full)
GList* get_playing_pids(void);

// Free a StreamInfo structure
void stream_info_free(StreamInfo *info);

// Set a device as the default sink
void set_default_audio_device(int device_index);

// Free an AudioDevice structure
void audio_device_free(AudioDevice *device);

#endif // AUDIO_H
