
/**
 * Copyright (C) 2009-2017 Fabio Leone
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * @file async.c
 * @brief Background operations
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <libgen.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include "gui.h"
#include "main.h"

extern Globals globals;
extern Prefs prefs;

// Access to ssh operations
pthread_mutex_t mutexSSH = PTHREAD_MUTEX_INITIALIZER;

time_t g_last_checkpoint_time;
GPtrArray *transferArray;

gboolean gIsTransferring;

void
lockSSH (char *caller, gboolean flagLock)
{
	if (flagLock) {
		log_debug ("[%s] locking SSH mutex...\n", caller);
		pthread_mutex_lock (&mutexSSH);
	} else {
		log_debug ("[%s] unlocking SSH mutex...\n", caller);
		pthread_mutex_unlock (&mutexSSH);
	}
}

time_t
checkpoint_get_last ()
{
	return (g_last_checkpoint_time);
}

void
checkpoint_update ()
{
	g_last_checkpoint_time = time (NULL);
}

void
asyncInit()
{
	// Init checkpoint
	checkpoint_update ();
	transferArray = g_ptr_array_new ();
	gIsTransferring = FALSE;
}

gpointer
async_lterm_loop (gpointer data)
{
	gint i;
	log_write ("%s BEGIN THREAD 0x%08x\n", __func__, pthread_self () );
	while (globals.running) {
		/* checkpoint */
		if (time (NULL) > checkpoint_get_last () + prefs.checkpoint_interval) {
			log_debug ("Checkpoint\n");
			/*
			#ifdef DEBUG
			        log_debug ("Current function: %s\n", getCurrentFunction ());
			#endif
			*/
			checkpoint_update ();
		}
		//log_debug ("Async iteration\n");
		// Check remote open files
		g_usleep (G_USEC_PER_SEC);
	}
	log_write ("%s END\n", __func__);
	return (NULL);
}

gboolean
async_is_transferring ()
{
	return gIsTransferring;
}

