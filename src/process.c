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

static char* get_icon_name_for_process(const char *process_name)
{
  // Simple mapping of common process names to icon names
  // This is a basic implementation - could be enhanced with desktop file parsing
  
  if (!process_name)
    return NULL;

  // Common applications mapping
  if (strstr(process_name, "chrome") || strstr(process_name, "google-chrome"))
    return g_strdup("google-chrome");
  if (strstr(process_name, "firefox"))
    return g_strdup("firefox");
  if (strstr(process_name, "code") || strstr(process_name, "vscode"))
    return g_strdup("code");
  if (strstr(process_name, "spotify"))
    return g_strdup("spotify");
  if (strstr(process_name, "discord"))
    return g_strdup("discord");
  if (strstr(process_name, "slack"))
    return g_strdup("slack");
  if (strstr(process_name, "telegram"))
    return g_strdup("telegram");
  if (strstr(process_name, "thunderbird"))
    return g_strdup("thunderbird");
  if (strstr(process_name, "nautilus"))
    return g_strdup("org.gnome.Nautilus");
  if (strstr(process_name, "gnome-terminal"))
    return g_strdup("org.gnome.Terminal");
  if (strstr(process_name, "konsole"))
    return g_strdup("konsole");
  if (strstr(process_name, "vlc"))
    return g_strdup("vlc");
  if (strstr(process_name, "gimp"))
    return g_strdup("gimp");
  
  // Default: use process name as icon name
  return g_strdup(process_name);
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
