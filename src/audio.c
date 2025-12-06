#include "audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>

// Structure to pass data to callbacks
typedef struct {
  GList *devices;
  pa_mainloop *mainloop;
  int operation_success;
  char *default_sink_name;
} UserData;

void audio_device_free(AudioDevice *device)
{
  if (device)
  {
    g_free(device->name);
    g_free(device);
  }
}

void stream_info_free(StreamInfo *info)
{
  if (info)
  {
    g_free(info);
  }
}

// Callback for sink info
static void sink_info_cb(pa_context *c, const pa_sink_info *i, int eol, void *userdata) {
  UserData *u = userdata;

  if (eol < 0) {
    u->operation_success = 0;
    return;
  }

  if (eol > 0) {
    return;
  }

  // Create new device
  AudioDevice *device = g_new0(AudioDevice, 1);
  device->index = i->index;
  
  // Use description for friendly name, fallback to name
  if (i->description)
    device->name = g_strdup(i->description);
  else
    device->name = g_strdup(i->name);

  // Check if this is the default sink
  if (u->default_sink_name && strcmp(i->name, u->default_sink_name) == 0) {
    device->is_default = 1;
  }

  // Calculate average volume percentage
  double vol_avg = (double)pa_cvolume_avg(&i->volume);
  device->volume = (int)((vol_avg / PA_VOLUME_NORM) * 100.0 + 0.5);
  if (device->volume > 100) device->volume = 100;

  u->devices = g_list_append(u->devices, device);
}

// Callback for server info (to get default sink name)
static void server_info_cb(pa_context *c, const pa_server_info *i, void *userdata) {
  UserData *u = userdata;
  if (i->default_sink_name) {
    u->default_sink_name = g_strdup(i->default_sink_name);
  }
}

GList* get_audio_devices(void) {
  pa_mainloop *m = NULL;
  pa_mainloop_api *api = NULL;
  pa_context *c = NULL;
  UserData u = {0};
  int ret;

  m = pa_mainloop_new();
  if (!m) return NULL;
  
  api = pa_mainloop_get_api(m);
  u.mainloop = m;
  u.operation_success = 1;

  c = pa_context_new(api, "BAM Audio Manager");
  if (!c) {
    pa_mainloop_free(m);
    return NULL;
  }

  if (pa_context_connect(c, NULL, 0, NULL) < 0) {
    pa_context_unref(c);
    pa_mainloop_free(m);
    return NULL;
  }

  // Wait for context to be ready
  while (pa_context_get_state(c) != PA_CONTEXT_READY) {
    if (pa_context_get_state(c) == PA_CONTEXT_FAILED || 
        pa_context_get_state(c) == PA_CONTEXT_TERMINATED) {
      pa_context_unref(c);
      pa_mainloop_free(m);
      return NULL;
    }
    pa_mainloop_iterate(m, 1, &ret);
  }

  // Get server info first to know default sink
  pa_operation *o = pa_context_get_server_info(c, server_info_cb, &u);
  if (o) {
    while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
      pa_mainloop_iterate(m, 1, &ret);
    }
    pa_operation_unref(o);
  }

  // Get sink info list
  o = pa_context_get_sink_info_list(c, sink_info_cb, &u);
  if (o) {
    while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
      pa_mainloop_iterate(m, 1, &ret);
    }
    pa_operation_unref(o);
  }

  pa_context_disconnect(c);
  pa_context_unref(c);
  pa_mainloop_free(m);
  
  g_free(u.default_sink_name);

  return u.devices;
}

void set_default_audio_device(int device_index) {
  char command[256];
  snprintf(command, sizeof(command), "pactl set-default-sink %d", device_index);
  system(command);
}

void set_device_volume(int device_index, int volume) {
  char command[256];
  snprintf(command, sizeof(command), "pactl set-sink-volume %d %d%%", device_index, volume);
  system(command);
}

// Callback for sink input info (active streams)
static void sink_input_info_cb(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata) {
  UserData *u = userdata;

  if (eol < 0) {
    u->operation_success = 0;
    return;
  }

  if (eol > 0) {
    return;
  }

  // Get PID from properties
  if (i->proplist) {
    const char *pid_str = pa_proplist_gets(i->proplist, PA_PROP_APPLICATION_PROCESS_ID);
    if (pid_str) {
      int pid = atoi(pid_str);
      if (pid > 0) {
        // Create StreamInfo with PID and sink index
        StreamInfo *stream = g_new0(StreamInfo, 1);
        stream->pid = pid;
        stream->sink_index = i->sink;
        stream->stream_index = i->index;
        u->devices = g_list_append(u->devices, stream);
      }
    }
  }
}

GList* get_playing_pids(void) {
  pa_mainloop *m = NULL;
  pa_mainloop_api *api = NULL;
  pa_context *c = NULL;
  UserData u = {0};
  int ret;

  m = pa_mainloop_new();
  if (!m) return NULL;
  
  api = pa_mainloop_get_api(m);
  u.mainloop = m;
  u.operation_success = 1;

  c = pa_context_new(api, "BAM Audio Manager");
  if (!c) {
    pa_mainloop_free(m);
    return NULL;
  }

  if (pa_context_connect(c, NULL, 0, NULL) < 0) {
    pa_context_unref(c);
    pa_mainloop_free(m);
    return NULL;
  }

  // Wait for context to be ready
  while (pa_context_get_state(c) != PA_CONTEXT_READY) {
    if (pa_context_get_state(c) == PA_CONTEXT_FAILED || 
        pa_context_get_state(c) == PA_CONTEXT_TERMINATED) {
      pa_context_unref(c);
      pa_mainloop_free(m);
      return NULL;
    }
    pa_mainloop_iterate(m, 1, &ret);
  }

  // Get sink input info list (active streams)
  pa_operation *o = pa_context_get_sink_input_info_list(c, sink_input_info_cb, &u);
  if (o) {
    while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
      pa_mainloop_iterate(m, 1, &ret);
    }
    pa_operation_unref(o);
  }

  pa_context_disconnect(c);
  pa_context_unref(c);
  pa_mainloop_free(m);
  
  return u.devices; // Here u.devices contains the list of PIDs
}

void kill_audio_stream(uint32_t stream_index) {
  pa_mainloop *m = NULL;
  pa_mainloop_api *api = NULL;
  pa_context *c = NULL;
  int ret;

  m = pa_mainloop_new();
  if (!m) return;
  
  api = pa_mainloop_get_api(m);
  
  // Use a different context name if desired, or same
  c = pa_context_new(api, "BAM Audio Killer");
  if (!c) {
    pa_mainloop_free(m);
    return;
  }

  if (pa_context_connect(c, NULL, 0, NULL) < 0) {
    pa_context_unref(c);
    pa_mainloop_free(m);
    return;
  }

  // Wait for context to be ready
  while (pa_context_get_state(c) != PA_CONTEXT_READY) {
    if (pa_context_get_state(c) == PA_CONTEXT_FAILED || 
        pa_context_get_state(c) == PA_CONTEXT_TERMINATED) {
      pa_context_unref(c);
      pa_mainloop_free(m);
      return;
    }
    pa_mainloop_iterate(m, 1, &ret);
  }

  // Kill the sink input (stream)
  pa_operation *o = pa_context_kill_sink_input(c, stream_index, NULL, NULL);
  if (o) {
    while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
      pa_mainloop_iterate(m, 1, &ret);
    }
    pa_operation_unref(o);
  }

  pa_context_disconnect(c);
  pa_context_unref(c);
  pa_mainloop_free(m);
}
