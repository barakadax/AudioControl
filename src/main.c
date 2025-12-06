#include <gtk/gtk.h>
#include "window.h"
#include "config.h"

static void activate(GtkApplication *app, gpointer user_data)
{
  create_and_setup_window(app);
}

int main(int argc, char *argv[])
{
  GtkApplication *app;
  int status;

  // Create a new GtkApplication instance
  app = gtk_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);

  // Connect the 'activate' signal to the activate function
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

  // Run the application (enters the main event loop)
  status = g_application_run(G_APPLICATION(app), argc, argv);

  // Free the application object when the loop exits
  g_object_unref(app);

  return status;
}
