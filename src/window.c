#include "window.h"
#include "callbacks.h"
#include "config.h"
#include "process.h"
#include "audio.h"

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#include <X11/Xlib.h>
#endif

// Structure to pass widgets to callbacks
typedef struct {
  GtkWidget *process_list;
  GtkWidget *audio_box;
  GtkWidget *filter_button;
  GtkWidget *search_entry;
  int device_card_width;
  int pid_list_height;
} AppWidgets;

// Structure for filter state
typedef struct {
  char *search_text;
  gboolean show_only_playing;
} FilterState;

// Filter function for process list
static gboolean filter_process_row(GtkListBoxRow *row, gpointer user_data)
{
  FilterState *state = (FilterState *)user_data;
  const char *search_text = state->search_text;
  gboolean show_only_playing = state->show_only_playing;
  
  GtkWidget *row_widget = gtk_list_box_row_get_child(row);
  ProcessInfo *info = g_object_get_data(G_OBJECT(row_widget), "process-info");
  
  if (!info)
    return TRUE;

  // Filter by playing status
  if (show_only_playing && !info->is_playing)
    return FALSE;

  if (!search_text || strlen(search_text) == 0)
    return TRUE; // Show all if no search text

  // Convert search text to lowercase for case-insensitive search
  char *search_lower = g_utf8_strdown(search_text, -1);
  char *name_lower = g_utf8_strdown(info->name, -1);
  
  // Check if search text matches PID or process name
  char pid_str[32];
  snprintf(pid_str, sizeof(pid_str), "%d", info->pid);
  
  gboolean matches = (strstr(pid_str, search_text) != NULL) ||
                     (strstr(name_lower, search_lower) != NULL);
  
  g_free(search_lower);
  g_free(name_lower);
  
  return matches;
}

// Callback when search text changes
static void on_search_changed(GtkSearchEntry *entry, gpointer user_data)
{
  AppWidgets *widgets = (AppWidgets *)user_data;
  GtkListBox *list_box = GTK_LIST_BOX(widgets->process_list);
  
  // Update filter state
  FilterState *state = g_object_get_data(G_OBJECT(list_box), "filter-state");
  if (state->search_text) g_free(state->search_text);
  state->search_text = g_strdup(gtk_editable_get_text(GTK_EDITABLE(entry)));
  
  gtk_list_box_invalidate_filter(list_box);
}

// Callback for filter button toggled
static void on_filter_playing_toggled(GtkButton *button, gpointer user_data)
{
  AppWidgets *widgets = (AppWidgets *)user_data;
  GtkListBox *list_box = GTK_LIST_BOX(widgets->process_list);
  
  // Update filter state
  FilterState *state = g_object_get_data(G_OBJECT(list_box), "filter-state");
  
  // Check if button is "active" (visual state handled by icon/label change or toggle button)
  // Here we use the button's user data to store state or check icon
  // But simpler: use GtkToggleButton instead of GtkButton for this
  
  state->show_only_playing = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
  
  gtk_list_box_invalidate_filter(list_box);
}

// Forward declaration
static void refresh_audio_list(AppWidgets *widgets);
static GdkContentProvider *on_drag_prepare(GtkDragSource *source, double x, double y, gpointer user_data);
static void load_css(void);

// Callback for refresh button
static void on_refresh_clicked(GtkButton *button, gpointer user_data)
{
  AppWidgets *widgets = (AppWidgets *)user_data;
  GtkListBox *list_box = GTK_LIST_BOX(widgets->process_list);
  
  // 1. Refresh Process List
  
  // Remove all existing rows
  GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(list_box));
  while (child)
  {
    GtkWidget *next = gtk_widget_get_next_sibling(child);
    gtk_list_box_remove(list_box, child);
    child = next;
  }
  
  // Reload processes
  GList *processes = get_user_processes();
  GList *playing_pids = get_playing_pids();
  for (GList *l = processes; l != NULL; l = l->next)
  {
    ProcessInfo *info = (ProcessInfo *)l->data;
    
    // Create a horizontal box for each row
    GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(row_box, 10);
    gtk_widget_set_margin_end(row_box, 10);
    gtk_widget_set_margin_top(row_box, 5);
    gtk_widget_set_margin_bottom(row_box, 5);

    // Store process info as data on the row for filtering
    ProcessInfo *row_info = g_new(ProcessInfo, 1);
    row_info->pid = info->pid;
    row_info->name = g_strdup(info->name);
    row_info->icon_name = info->icon_name ? g_strdup(info->icon_name) : NULL;

    // Check if playing
    row_info->is_playing = 0;
    uint32_t stream_idx = 0;
    for (GList *p = playing_pids; p != NULL; p = p->next) {
      StreamInfo *stream = (StreamInfo *)p->data;
      if (stream->pid == info->pid) {
        row_info->is_playing = 1;
        stream_idx = stream->stream_index;
        break;
      }
    }

    g_object_set_data_full(G_OBJECT(row_box), "process-info", row_info, (GDestroyNotify)process_info_free);

    // --- DRAG SOURCE SETUP ---
    if (row_info->is_playing) {
      GtkDragSource *drag_source = gtk_drag_source_new();
      g_signal_connect(drag_source, "prepare", G_CALLBACK(on_drag_prepare), GUINT_TO_POINTER(stream_idx));
      gtk_widget_add_controller(row_box, GTK_EVENT_CONTROLLER(drag_source));
    }

    // Icon
    GtkWidget *icon = NULL;
    if (info->icon_name)
    {
      icon = gtk_image_new_from_icon_name(info->icon_name);
      gtk_image_set_pixel_size(GTK_IMAGE(icon), 24);
      gtk_box_append(GTK_BOX(row_box), icon);
    }

    // Sound icon
    GtkWidget *sound_icon;
    if (row_info->is_playing) {
      sound_icon = gtk_image_new_from_icon_name("audio-volume-high-symbolic");
    } else {
      sound_icon = gtk_image_new_from_icon_name("audio-volume-muted-symbolic");
      gtk_widget_set_opacity(sound_icon, 0.5);
    }
    gtk_image_set_pixel_size(GTK_IMAGE(sound_icon), 16);
    gtk_widget_set_margin_start(sound_icon, 5);
    gtk_widget_set_margin_end(sound_icon, 5);
    gtk_box_append(GTK_BOX(row_box), sound_icon);

    // PID label
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", info->pid);
    GtkWidget *pid_label = gtk_label_new(pid_str);
    // Removed fixed size request to remove space
    // gtk_widget_set_size_request(pid_label, 60, -1); 
    gtk_label_set_xalign(GTK_LABEL(pid_label), 0.0);

    // Process name label
    GtkWidget *name_label = gtk_label_new(info->name);
    gtk_label_set_xalign(GTK_LABEL(name_label), 0.0);
    gtk_widget_set_hexpand(name_label, TRUE);

    // Add labels to row
    gtk_box_append(GTK_BOX(row_box), pid_label);
    gtk_box_append(GTK_BOX(row_box), name_label);

    // Add row to list box
    gtk_list_box_append(GTK_LIST_BOX(list_box), row_box);
  }
  
  // Free process list
  g_list_free_full(processes, (GDestroyNotify)process_info_free);
  g_list_free_full(playing_pids, (GDestroyNotify)stream_info_free);
  
  // 2. Refresh Audio List
  refresh_audio_list(widgets);
}

// Callback for volume slider changes
static void on_volume_changed(GtkRange *range, gpointer user_data)
{
  int device_index = GPOINTER_TO_INT(user_data);
  int volume = (int)gtk_range_get_value(range);
  set_device_volume(device_index, volume);
}

// Callback for default checkbox toggled
static void on_default_toggled(GtkCheckButton *button, gpointer user_data)
{
  // Only react if the button became active (checked)
  if (gtk_check_button_get_active(button)) {
    int device_index = GPOINTER_TO_INT(user_data);
    set_default_audio_device(device_index);
  }
}

// Callback for stream row activation (double-click)
static void on_stream_row_activated(GtkListBox *list_box, GtkListBoxRow *row, gpointer user_data)
{
  AppWidgets *widgets = (AppWidgets *)user_data;
  GtkWidget *child = gtk_list_box_row_get_child(row);
  
  // Get stream index
  gpointer data = g_object_get_data(G_OBJECT(child), "stream-index");
  if (data != NULL) {
    uint32_t stream_index = GPOINTER_TO_UINT(data);
    kill_audio_stream(stream_index);
    
    // Refresh to show it's gone
    refresh_audio_list(widgets);
  }
}


// -- Drag and Drop Callbacks --

static GdkContentProvider *on_drag_prepare(GtkDragSource *source, double x, double y, gpointer user_data)
{
  uint32_t stream_index = GPOINTER_TO_UINT(user_data);
  // Pass stream index as a string or byte array
  return gdk_content_provider_new_typed(G_TYPE_UINT, stream_index);
}

static gboolean on_drop(GtkDropTarget *target, const GValue *value, double x, double y, gpointer user_data)
{
  uint32_t sink_index = GPOINTER_TO_UINT(user_data);
  
  if (G_VALUE_HOLDS(value, G_TYPE_UINT)) {
    uint32_t stream_index = g_value_get_uint(value);
    
    // Move the stream
    move_audio_stream(stream_index, sink_index);
    
    // Refresh UI (we need the widgets structure, which we can attach to the controller or lookup)
    // For simplicity, we can just trigger a refresh if we could access widgets.
    // Ideally we should pass widgets to this callback, but we need sink_index too.
    // Let's rely on the user manually refreshing or auto-refresh if we had a timer.
    // BUT the requirement says "drop it... so it will be added...". Visual feedback is key.
    
    // To cleanly refresh, we can walk up the widget tree to find the window and get "app-widgets" data
    GtkWidget *widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(target));
    GtkWidget *window = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);
    if (window) {
        AppWidgets *widgets = g_object_get_data(G_OBJECT(window), "app-widgets");
        if (widgets) {
             // Small delay to allow PA to process the move
             // g_timeout_add(100, (GSourceFunc)refresh_audio_list_wrapper, widgets); 
             // or just direct refresh:
             on_refresh_clicked(NULL, widgets);
        }
    }

    return TRUE;
  }
  return FALSE;
}

static void refresh_audio_list(AppWidgets *widgets)
{
  GtkWidget *audio_box = widgets->audio_box;
  // Remove all children first
  GtkWidget *child = gtk_widget_get_first_child(audio_box);
  while (child != NULL) {
    GtkWidget *next = gtk_widget_get_next_sibling(child);
    gtk_box_remove(GTK_BOX(audio_box), child);
    child = next;
  }

  // Get audio devices and create cards
  GList *audio_devices = get_audio_devices();
  guint device_count = g_list_length(audio_devices);
  GtkCheckButton *first_radio = NULL; // To store the first radio for grouping

  if (device_count == 0)
  {
    GtkWidget *no_device_label = gtk_label_new("No audio devices found");
    gtk_widget_set_halign(no_device_label, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(no_device_label, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(no_device_label, TRUE);
    gtk_widget_set_margin_top(no_device_label, 20);
    gtk_widget_set_margin_bottom(no_device_label, 20);
    gtk_box_append(GTK_BOX(audio_box), no_device_label);
  }
  else
  {
    for (GList *l = audio_devices; l != NULL; l = l->next)
    {
      AudioDevice *device = (AudioDevice *)l->data;

      // Add separator before every card except the first one
      if (l != audio_devices)
      {
        GtkWidget *v_separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
        gtk_widget_set_margin_start(v_separator, 5);
        gtk_widget_set_margin_end(v_separator, 5);
        gtk_box_append(GTK_BOX(audio_box), v_separator);
      }

      // Create a card for each device
      GtkWidget *device_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
      gtk_widget_set_size_request(device_card, widgets->device_card_width, -1);
      gtk_widget_set_hexpand(device_card, FALSE); // Prevent expansion
      gtk_widget_add_css_class(device_card, "card");
      
      // Make device card a Drop Target
      GtkDropTarget *drop_target = gtk_drop_target_new(G_TYPE_UINT, GDK_ACTION_COPY | GDK_ACTION_MOVE);
      g_signal_connect(drop_target, "drop", G_CALLBACK(on_drop), GUINT_TO_POINTER(device->index));
      gtk_widget_add_controller(device_card, GTK_EVENT_CONTROLLER(drop_target));


      // Device name label
      GtkWidget *name_label = gtk_label_new(device->name);
      gtk_label_set_wrap(GTK_LABEL(name_label), TRUE);
      // Approx chars for width (assuming ~10px per char average or just a rough estimate)
      gtk_label_set_max_width_chars(GTK_LABEL(name_label), widgets->device_card_width / 10);
      gtk_widget_set_margin_start(name_label, 5);
      gtk_widget_set_margin_end(name_label, 5);
      gtk_widget_set_margin_top(name_label, 5);

      // Volume slider
      GtkWidget *volume_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
      gtk_range_set_value(GTK_RANGE(volume_slider), device->volume);
      gtk_scale_set_draw_value(GTK_SCALE(volume_slider), FALSE);
      gtk_widget_set_hexpand(volume_slider, TRUE);
      gtk_widget_set_margin_start(volume_slider, 5);
      gtk_widget_set_margin_end(volume_slider, 5);
      gtk_widget_set_margin_bottom(volume_slider, 5);

      // Connect volume slider to callback
      g_signal_connect(volume_slider, "value-changed", 
                       G_CALLBACK(on_volume_changed), 
                       GINT_TO_POINTER((int)device->index));

      // Add widgets to card
      gtk_box_append(GTK_BOX(device_card), name_label);
      gtk_box_append(GTK_BOX(device_card), volume_slider);
      
      // Default device checkbox (Radio button behavior)
      GtkWidget *default_check = gtk_check_button_new_with_label("Default Output");
      gtk_widget_set_margin_start(default_check, 5);
      gtk_widget_set_margin_end(default_check, 5);
      gtk_widget_set_margin_bottom(default_check, 5);
      
      // Grouping logic
      if (first_radio == NULL) {
        first_radio = GTK_CHECK_BUTTON(default_check);
      } else {
        gtk_check_button_set_group(GTK_CHECK_BUTTON(default_check), first_radio);
      }
      
      // Set active state
      gtk_check_button_set_active(GTK_CHECK_BUTTON(default_check), device->is_default);
      
      // Disable if only one device
      if (device_count == 1) {
        gtk_widget_set_sensitive(default_check, FALSE);
      }
      
      // Connect signal
      g_signal_connect(default_check, "toggled", 
                       G_CALLBACK(on_default_toggled), 
                       GINT_TO_POINTER((int)device->index));
                       
      gtk_box_append(GTK_BOX(device_card), default_check);

      // --- PID List Section ---
      
      // Create a scrolled window for the list
      GtkWidget *scrolled_window = gtk_scrolled_window_new();
      gtk_widget_set_size_request(scrolled_window, -1, widgets->pid_list_height);
      gtk_widget_set_vexpand(scrolled_window, TRUE);
      gtk_widget_set_margin_start(scrolled_window, 5);
      gtk_widget_set_margin_end(scrolled_window, 5);
      gtk_widget_set_margin_bottom(scrolled_window, 5);
      
      // Create list box
      GtkWidget *process_list_box = gtk_list_box_new();
      gtk_list_box_set_selection_mode(GTK_LIST_BOX(process_list_box), GTK_SELECTION_NONE);
      gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(process_list_box), FALSE);
      g_signal_connect(process_list_box, "row-activated", G_CALLBACK(on_stream_row_activated), widgets);
      gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), process_list_box);
      
      // Get processes and populate list (only playing on this device)
      GList *streams = get_playing_pids();
      for (GList *p = streams; p != NULL; p = p->next) {
        StreamInfo *stream = (StreamInfo *)p->data;
        
        // Only show PIDs playing to this specific device
        if (stream->sink_index != device->index) {
          continue;
        }
        
        // Get process info for this PID
        GList *all_processes = get_user_processes();
        ProcessInfo *proc_info = NULL;
        for (GList *proc = all_processes; proc != NULL; proc = proc->next) {
          ProcessInfo *info = (ProcessInfo *)proc->data;
          if (info->pid == stream->pid) {
            proc_info = info;
            break;
          }
        }
        
        if (proc_info) {
          GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
          g_object_set_data(G_OBJECT(row_box), "stream-index", GUINT_TO_POINTER(stream->stream_index));
          
          char pid_str[32];
          snprintf(pid_str, sizeof(pid_str), "%d", stream->pid);
          GtkWidget *pid_label = gtk_label_new(pid_str);
          gtk_widget_set_size_request(pid_label, 50, -1);
          gtk_widget_set_halign(pid_label, GTK_ALIGN_START);
          
          GtkWidget *name_label = gtk_label_new(proc_info->name);
          gtk_label_set_ellipsize(GTK_LABEL(name_label), PANGO_ELLIPSIZE_END);
          gtk_widget_set_halign(name_label, GTK_ALIGN_START);
          
          gtk_box_append(GTK_BOX(row_box), pid_label);
          gtk_box_append(GTK_BOX(row_box), name_label);
          
          gtk_list_box_append(GTK_LIST_BOX(process_list_box), row_box);
        }
        
        g_list_free_full(all_processes, (GDestroyNotify)process_info_free);
      }
      g_list_free_full(streams, (GDestroyNotify)stream_info_free);
      
      gtk_box_append(GTK_BOX(device_card), scrolled_window);
      
      // ------------------------

      // Add card to audio box
      gtk_box_append(GTK_BOX(audio_box), device_card);
    }
  }
  
  g_list_free_full(audio_devices, (GDestroyNotify)audio_device_free);
}

void create_and_setup_window(GtkApplication *app)
{
  // Create a new window
  GtkWidget *window = gtk_application_window_new(app);

  // Load CSS
  load_css();

  // Get the display and monitors to calculate size
  GdkDisplay *display = gtk_widget_get_display(window);
  GListModel *monitors = gdk_display_get_monitors(display);

  int target_width = 800;  // Fallback
  int target_height = 600; // Fallback
  GdkRectangle geometry = {0};

  if (g_list_model_get_n_items(monitors) > 0)
  {
    // Get the first monitor
    GdkMonitor *monitor = GDK_MONITOR(g_list_model_get_item(monitors, 0));
    gdk_monitor_get_geometry(monitor, &geometry);

    target_width = (int)(geometry.width * WINDOW_WIDTH_SIZE_RATIO);
    target_height = (int)(geometry.height * WINDOW_HEIGHT_SIZE_RATIO);

    gtk_window_set_default_size(GTK_WINDOW(window), target_width, target_height);

    // g_list_model_get_item returns a full reference, so we must unref it
    g_object_unref(monitor);
  }

  // Set the title of the window
  gtk_window_set_title(GTK_WINDOW(window), APP_TITLE);

  // Make it "headless" (undecorated)
  gtk_window_set_decorated(GTK_WINDOW(window), FALSE);

  // Connect to the "notify::is-active" signal to detect focus loss
  g_signal_connect(window, "notify::is-active", G_CALLBACK(on_window_active_changed), app);

  // Create a paned widget to split the window vertically (top and bottom)
  GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
  gtk_widget_set_hexpand(paned, TRUE);
  gtk_widget_set_vexpand(paned, TRUE);

  // Create a container for the search box and process list
  GtkWidget *top_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_widget_set_hexpand(top_container, TRUE);
  gtk_widget_set_vexpand(top_container, TRUE);

  // Create horizontal box for refresh button and search entry
  GtkWidget *search_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_widget_set_margin_start(search_box, 10);
  gtk_widget_set_margin_end(search_box, 10);
  gtk_widget_set_margin_top(search_box, 10);

  // Create refresh button
  GtkWidget *refresh_button = gtk_button_new_from_icon_name("view-refresh-symbolic");
  gtk_widget_set_tooltip_text(refresh_button, "Refresh process list");

  // Create filter button (toggle)
  GtkWidget *filter_button = gtk_toggle_button_new();
  gtk_button_set_icon_name(GTK_BUTTON(filter_button), "audio-volume-high-symbolic"); // Or another icon
  gtk_widget_set_tooltip_text(filter_button, "Show only playing processes");

  // Create search entry
  GtkWidget *search_entry = gtk_search_entry_new();
  gtk_widget_set_hexpand(search_entry, TRUE);
  gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(search_entry), "Search by PID or process name...");

  // Add refresh button and search entry to search box
  gtk_box_append(GTK_BOX(search_box), refresh_button);
  gtk_box_append(GTK_BOX(search_box), filter_button);
  gtk_box_append(GTK_BOX(search_box), search_entry);

  // Create scrolled window for process list
  GtkWidget *scrolled_window = gtk_scrolled_window_new();
  gtk_widget_set_hexpand(scrolled_window, TRUE);
  gtk_widget_set_vexpand(scrolled_window, TRUE);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);

  // Create list box for processes
  GtkWidget *list_box = gtk_list_box_new();
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), list_box);

  // Set up filter function for search
  // Create filter state
  FilterState *filter_state = g_new0(FilterState, 1);
  g_object_set_data_full(G_OBJECT(list_box), "filter-state", filter_state, g_free);

  // Set up filter function for search
  gtk_list_box_set_filter_func(GTK_LIST_BOX(list_box),
                                 filter_process_row,
                                 filter_state, NULL);

  // Get user processes and populate list
  GList *processes = get_user_processes();
  GList *playing_pids = get_playing_pids();
  for (GList *l = processes; l != NULL; l = l->next)
  {
    ProcessInfo *info = (ProcessInfo *)l->data;
    
    // Create a horizontal box for each row
    GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(row_box, 10);
    gtk_widget_set_margin_end(row_box, 10);
    gtk_widget_set_margin_top(row_box, 5);
    gtk_widget_set_margin_bottom(row_box, 5);

    // Store process info as data on the row for filtering
    ProcessInfo *row_info = g_new(ProcessInfo, 1);
    row_info->pid = info->pid;
    row_info->name = g_strdup(info->name);
    row_info->icon_name = info->icon_name ? g_strdup(info->icon_name) : NULL;

    // Check if playing
    row_info->is_playing = 0;
    uint32_t stream_idx = 0; // Default 0 (invalid usually for PA but we handle logic carefully)
    for (GList *p = playing_pids; p != NULL; p = p->next) {
      StreamInfo *stream = (StreamInfo *)p->data;
      if (stream->pid == info->pid) {
        row_info->is_playing = 1;
        stream_idx = stream->stream_index;
        break;
      }
    }

    g_object_set_data_full(G_OBJECT(row_box), "process-info", row_info, (GDestroyNotify)process_info_free);

    // --- DRAG SOURCE SETUP ---
    if (row_info->is_playing) {
      GtkDragSource *drag_source = gtk_drag_source_new();
      g_signal_connect(drag_source, "prepare", G_CALLBACK(on_drag_prepare), GUINT_TO_POINTER(stream_idx));
      // Optionally set an icon or widget for drag feedback
      gtk_widget_add_controller(row_box, GTK_EVENT_CONTROLLER(drag_source));
    }

    // Icon
    GtkWidget *icon = NULL;
    if (info->icon_name)
    {
      icon = gtk_image_new_from_icon_name(info->icon_name);
      gtk_image_set_pixel_size(GTK_IMAGE(icon), 24);
      gtk_box_append(GTK_BOX(row_box), icon);
    }

    // Sound icon
    GtkWidget *sound_icon;
    if (row_info->is_playing) {
      sound_icon = gtk_image_new_from_icon_name("audio-volume-high-symbolic");
    } else {
      sound_icon = gtk_image_new_from_icon_name("audio-volume-muted-symbolic");
      gtk_widget_set_opacity(sound_icon, 0.5);
    }
    gtk_image_set_pixel_size(GTK_IMAGE(sound_icon), 16);
    gtk_widget_set_margin_start(sound_icon, 5);
    gtk_widget_set_margin_end(sound_icon, 5);
    gtk_box_append(GTK_BOX(row_box), sound_icon);

    // PID label
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", info->pid);
    GtkWidget *pid_label = gtk_label_new(pid_str);
    // Removed fixed size request to remove space
    // gtk_widget_set_size_request(pid_label, 60, -1); 
    gtk_label_set_xalign(GTK_LABEL(pid_label), 0.0);

    // Process name label
    GtkWidget *name_label = gtk_label_new(info->name);
    gtk_label_set_xalign(GTK_LABEL(name_label), 0.0);
    gtk_widget_set_hexpand(name_label, TRUE);

    // Add labels to row
    gtk_box_append(GTK_BOX(row_box), pid_label);
    gtk_box_append(GTK_BOX(row_box), name_label);

    // Add row to list box
    gtk_list_box_append(GTK_LIST_BOX(list_box), row_box);
  }

  // Free process list
  g_list_free_full(processes, (GDestroyNotify)process_info_free);
  g_list_free_full(playing_pids, (GDestroyNotify)stream_info_free);

  // Add search box and scrolled window to top container
  gtk_box_append(GTK_BOX(top_container), search_box);
  gtk_box_append(GTK_BOX(top_container), scrolled_window);

  // Create horizontal scrolled window for audio devices
  GtkWidget *audio_scrolled = gtk_scrolled_window_new();
  gtk_widget_set_hexpand(audio_scrolled, TRUE);
  gtk_widget_set_vexpand(audio_scrolled, TRUE);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(audio_scrolled),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_NEVER);

  // Create horizontal box for audio devices
  GtkWidget *audio_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_margin_start(audio_box, 10);
  gtk_widget_set_margin_end(audio_box, 10);
  gtk_widget_set_margin_top(audio_box, 10);
  gtk_widget_set_margin_bottom(audio_box, 10);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(audio_scrolled), audio_box);

  // Create widgets struct to pass to callbacks
  AppWidgets *widgets = g_new(AppWidgets, 1);
  widgets->process_list = list_box;
  widgets->audio_box = audio_box;
  widgets->filter_button = filter_button;
  widgets->search_entry = search_entry;
  
  // Calculate relative sizes
  widgets->device_card_width = (int)(target_width * DEVICE_CARD_WIDTH_RATIO);
  widgets->pid_list_height = (int)(target_height * PID_LIST_HEIGHT_RATIO);
  
  // Attach it to the window to free it later
  g_object_set_data_full(G_OBJECT(window), "app-widgets", widgets, g_free);

  // Connect search entry to filter function
  g_signal_connect(search_entry, "search-changed", G_CALLBACK(on_search_changed), widgets);
  
  // Connect filter button
  g_signal_connect(filter_button, "toggled", G_CALLBACK(on_filter_playing_toggled), widgets);
  
  // Connect refresh button
  g_signal_connect(refresh_button, "clicked", G_CALLBACK(on_refresh_clicked), widgets);



  // Get audio devices and create cards
  refresh_audio_list(widgets);

  // Add widgets to the paned
  gtk_paned_set_start_child(GTK_PANED(paned), top_container);
  gtk_paned_set_end_child(GTK_PANED(paned), audio_scrolled);
  gtk_paned_set_position(GTK_PANED(paned), target_height / 2);

  // Place the paned inside the window
  gtk_window_set_child(GTK_WINDOW(window), paned);

  // Display the window
  gtk_window_present(GTK_WINDOW(window));

  // Attempt to center on X11 - must be done after window is realized
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY(display))
  {
    // Force window realization
    gtk_widget_realize(window);
    
    GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(window));
    if (surface)
    {
      int x = geometry.x + (geometry.width - target_width) / 2;
      int y = geometry.y + (geometry.height - target_height) / 2;

      Display *xdisplay = gdk_x11_display_get_xdisplay(display);
      Window xid = gdk_x11_surface_get_xid(surface);

      XMoveWindow(xdisplay, xid, x, y);
      XFlush(xdisplay);  // Ensure the move is processed
    }
  }
#endif
}

// Helper to load CSS
static void load_css(void)
{
  GtkCssProvider *provider = gtk_css_provider_new();
  const char *css_path = "src/style.css"; // Relative path for dev, or absolute. Best is resource.
  // For this setup, we assume running from project root.
  
  // Try absolute path reconstruction if needed, but relative usually works if CWD is correct.
  // Let's use relative "src/style.css"
  
  gtk_css_provider_load_from_path(provider, css_path);
  
  gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                             GTK_STYLE_PROVIDER(provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  
  g_object_unref(provider);
}

