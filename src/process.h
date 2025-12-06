#ifndef PROCESS_H
#define PROCESS_H

#include <glib.h>

typedef struct {
  int pid;
  char *name;
  char *icon_name;  // Icon name for themed icon
  int is_playing; // 1 if playing audio, 0 otherwise
} ProcessInfo;

// Get list of user processes (caller must free with g_list_free_full)
GList* get_user_processes(void);

// Free a ProcessInfo structure
void process_info_free(ProcessInfo *info);

#endif // PROCESS_H
