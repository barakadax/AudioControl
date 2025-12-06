#include "callbacks.h"

void on_window_active_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
  GtkWindow *window = GTK_WINDOW(object);
  GtkApplication *app = GTK_APPLICATION(user_data);

  if (!gtk_window_is_active(window))
  {
    g_application_quit(G_APPLICATION(app));
  }
}
