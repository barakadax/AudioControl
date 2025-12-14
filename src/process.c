#include "process.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>

void process_info_free(ProcessInfo *info)
{
  if (info)
  {
    g_free(info->name);
    g_free(info->icon_name);
    g_free(info);
  }
}

static int is_numeric(const char *str)
{
  while (*str)
  {
    if (!isdigit(*str))
      return 0;
    str++;
  }
  return 1;
}

static char* read_process_name(int pid)
{
  char path[256];
  char buffer[256];
  FILE *fp;

  // Try to read from /proc/[pid]/comm first (cleaner name)
  snprintf(path, sizeof(path), "/proc/%d/comm", pid);
  fp = fopen(path, "r");
  if (fp)
  {
    if (fgets(buffer, sizeof(buffer), fp))
    {
      // Remove newline
      buffer[strcspn(buffer, "\n")] = 0;
      fclose(fp);
      return g_strdup(buffer);
    }
    fclose(fp);
  }

  // Fallback to cmdline
  snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
  fp = fopen(path, "r");
  if (fp)
  {
    if (fgets(buffer, sizeof(buffer), fp))
    {
      // cmdline uses null bytes as separators, just take first part
      fclose(fp);
      return g_strdup(buffer);
    }
    fclose(fp);
  }

  return g_strdup("unknown");
}

static int is_user_process(int pid)
{
  char path[256];
  struct stat st;

  snprintf(path, sizeof(path), "/proc/%d", pid);
  if (stat(path, &st) == 0)
  {
    // Filter: only show processes owned by users (UID >= 1000)
    // This excludes root and system processes
    return st.st_uid >= 1000;
  }
  return 0;
}

static GHashTable *icon_cache = NULL;

static char* get_icon_from_desktop_file(const char *path)
{
  FILE *fp = fopen(path, "r");
  if (!fp) return NULL;

  char buffer[512];
  char *icon_name = NULL;

  while (fgets(buffer, sizeof(buffer), fp))
  {
    if (strncmp(buffer, "Icon=", 5) == 0)
    {
      icon_name = g_strdup(buffer + 5);
      // Remove newline
      icon_name[strcspn(icon_name, "\n")] = 0;
      break;
    }
  }
  fclose(fp);
  return icon_name;
}

// Check if a desktop file's Exec line matches the process name
static int check_desktop_file_for_process(const char *path, const char *process_name)
{
  FILE *fp = fopen(path, "r");
  if (!fp) return 0;

  char buffer[512];
  int found = 0;

  while (fgets(buffer, sizeof(buffer), fp))
  {
    if (strncmp(buffer, "Exec=", 5) == 0)
    {
      // Check if Exec line contains process name
      // Logic: Exec=/usr/bin/appname %U -> we check if "appname" is present
      // To be more precise, we could check for process_name surrounded by delimiters strings
      // But strstr is a good first step for "contains"
      char *exec_cmd = buffer + 5;
      if (strstr(exec_cmd, process_name))
      {
        found = 1;
        break;
      }
    }
  }
  fclose(fp);
  return found;
}

static char* find_desktop_file(const char *process_name)
{
  const char *home = getenv("HOME");
  char local_apps[1024];
  if (home) {
    snprintf(local_apps, sizeof(local_apps), "%s/.local/share/applications", home);
  } else {
    local_apps[0] = 0;
  }

  const char *dirs[] = {
    "/usr/share/applications",
    "/usr/local/share/applications",
    local_apps,
    ".",
    NULL
  };
  
  // First try direct match: process_name.desktop
  char path[1024];
  for (int i = 0; dirs[i] && dirs[i][0]; i++) {
    snprintf(path, sizeof(path), "%s/%s.desktop", dirs[i], process_name);
    if (access(path, F_OK) == 0) {
      return g_strdup(path);
    }
  }

  // If not found, iterate directories to find Exec=Match
  for (int i = 0; dirs[i] && dirs[i][0]; i++) {
    DIR *d = opendir(dirs[i]);
    if (!d) continue;
    
    struct dirent *entry;
    while ((entry = readdir(d))) {
      if (strstr(entry->d_name, ".desktop")) {
        snprintf(path, sizeof(path), "%s/%s", dirs[i], entry->d_name);
        if (check_desktop_file_for_process(path, process_name)) {
          closedir(d);
          return g_strdup(path);
        }
      }
    }
    closedir(d);
  }
  
  return NULL;
}

static char* get_icon_name_for_process(const char *process_name)
{
  if (!process_name)
    return NULL;

  if (!icon_cache)
    icon_cache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

  // Check cache
  char *cached_icon = g_hash_table_lookup(icon_cache, process_name);
  if (cached_icon)
    return g_strdup(cached_icon);

  // Try to find desktop file
  char *desktop_path = find_desktop_file(process_name);
  char *found_icon = NULL;

  if (desktop_path) {
    found_icon = get_icon_from_desktop_file(desktop_path);
    g_free(desktop_path);
  }

  // If still not found, fallback to process name itself
  if (!found_icon) {
    found_icon = g_strdup(process_name);
  }

  // Store in cache (store a copy)
  g_hash_table_insert(icon_cache, g_strdup(process_name), g_strdup(found_icon));

  return found_icon;
}

GList* get_user_processes(void)
{
  DIR *dir;
  struct dirent *entry;
  GList *processes = NULL;

  dir = opendir("/proc");
  if (!dir)
    return NULL;

  while ((entry = readdir(dir)) != NULL)
  {
    // Check if directory name is numeric (PID)
    if (entry->d_type == DT_DIR && is_numeric(entry->d_name))
    {
      int pid = atoi(entry->d_name);
      
      // Filter to user processes only
      if (is_user_process(pid))
      {
        ProcessInfo *info = g_new(ProcessInfo, 1);
        info->pid = pid;
        info->name = read_process_name(pid);
        info->icon_name = get_icon_name_for_process(info->name);
        processes = g_list_append(processes, info);
      }
    }
  }

  closedir(dir);
  return processes;
}
