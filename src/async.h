
#ifndef _ASYNC_H
#define _ASYNC_H

#include <gtk/gtk.h>

void asyncInit();
gpointer async_lterm_loop (gpointer data);

gboolean async_is_transferring ();

#endif

