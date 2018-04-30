
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
 * @file main.c
 * @brief The main file
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <locale.h>
#include <libgen.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include <vte/vte.h>
#include <libssh/libssh.h>
#include <libssh/callbacks.h>
#include <pwd.h>
#include "main.h"
#include "gui.h"
#include "profile.h"
#include "connection.h"
#include "ssh.h"
#include "utils.h"
#include "config.h"

int switch_local = 0;     /* start with local shell */

Globals globals;
Prefs prefs;
struct Protocol g_ssh_prot = { "ssh", "-p %p -l %u %h", 22, PROT_FLAG_ASKPASSWORD };
struct Profile g_profile;
GApplication *application;

void load_settings ();
void save_settings ();
void show_version ();
void help ();

// Access to ssh operations
pthread_mutex_t mutexSSH = PTHREAD_MUTEX_INITIALIZER;

void
lockSSH (const char *caller, gboolean flagLock)
{
	if (flagLock) {
		log_debug ("[%s] locking SSH mutex...\n", caller);
		pthread_mutex_lock (&mutexSSH);
	} else {
		log_debug ("[%s] unlocking SSH mutex...\n", caller);
		pthread_mutex_unlock (&mutexSSH);
	}
}

void *malloc ();

/* Allocate an N-byte block of memory from the heap.
   If N is zero, allocate a 1-byte block.  */

void* rpl_malloc (size_t n)
{
	if (n == 0)
		n = 1;
	return malloc (n);
}

void
log_reset ()
{
	FILE *log_fp;
	log_fp = fopen (globals.log_file, "w");
	if (log_fp == NULL)
		return;
	fclose (log_fp);
}

void
log_write (const char *fmt, ...)
{
	FILE *log_fp;
	char time_s[64];
	char log_file[256];
	char line[2048];
	char msg[2048];
	time_t tmx;
	struct tm *tml;
	va_list ap;
	tmx = time (NULL);
	tml = localtime (&tmx);
	log_fp = fopen (globals.log_file, "a");
	if (log_fp == NULL)
		return;
	strftime (time_s, sizeof (time_s), "%Y-%m-%d %H:%M:%S", tml);
	va_start (ap, fmt);
	vsprintf (msg, fmt, ap);
	va_end (ap);
	sprintf (line, "%s: %s", time_s, msg);
	fprintf (log_fp, "%s", line);
	fflush (log_fp);
	fclose (log_fp);
}

static void
sigterm_handler (int signalnum, siginfo_t *si, void *data)
{
	printf ("Caught %d signal, exiting...\n", signalnum);
	application_quit ();
}

static void handle_signals (void)
{
	struct sigaction sa;
	sa.sa_sigaction = sigterm_handler;
	sigemptyset (&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;
	sigaction (SIGTERM, &sa, NULL);
	//sigaction (SIGINT,  &sa, 0);
}

gboolean
doGTKMainIteration ()
{
	//log_debug ("[thread 0x%08x] Start\n", pthread_self());
	while (gtk_events_pending () )
		gtk_main_iteration ();
	//log_debug ("[thread 0x%08x] End\n", pthread_self());
	return (G_SOURCE_REMOVE);
}

void
addIdleGTKMainIteration ()
{
	gdk_threads_add_idle (doGTKMainIteration, NULL);
}

void
lterm_iteration ()
{
	struct Iteration_Function_Request ifr_function;
	struct ConnectionTab *lterminal;
	//log_debug ("Start\n");
	doGTKMainIteration ();
	while (ifr_get (&ifr_function) ) {
		log_debug ("Requested iteration function: %d\n", ifr_function.id);
		switch (ifr_function.id) {
			case ITERATION_REBUILD_TREE_STORE:
				rebuild_tree_store ();
				break;
			case ITERATION_REFRESH_TREE_VIEW:
				refresh_connection_tree_view ( (GtkTreeView *) ifr_function.user_data);
				break;
			case ITERATION_CLOSE_TAB:
				connection_tab_close ( (SConnectionTab *) ifr_function.user_data);
				break;
			default:
				break;
		} /* switch */
	}
	/* prevent from 100% cpu usage */
	g_usleep (1000);
	//log_debug ("End\n");
}

int sTimeout = 0;
pthread_t tidAlarm = 0; // Thread that must receive alarms

// A thread requests receive alarm
void
threadRequestAlarm ()
{
	tidAlarm = pthread_self ();
	log_debug ("0x%08x\n", tidAlarm);
}

void
threadResetAlarm ()
{
	tidAlarm = 0;
}

void
AlarmHandler (int sig)
{
	log_debug ("[thread 0x%08x] %d\n", pthread_self (), sig);
	sTimeout = 1;
	if (tidAlarm && tidAlarm != pthread_self () ) {
		log_debug ("Send alarm to thread 0x%08x\n", tidAlarm);
		pthread_kill (tidAlarm, sig);
	}
}

void
timerStart (int seconds)
{
	signal (SIGALRM, AlarmHandler);
	sTimeout = 0;
	alarm (seconds);
	log_debug ("Timer started: %d\n", seconds);
}

void
timerStop ()
{
	signal (SIGALRM, SIG_DFL);
	sTimeout = 0;
	alarm (0);
	log_debug ("Timer stopped\n");
}

int timedOut ()
{
	return (sTimeout == 1);
}

int
main (int argc, char *argv[])
{
	int rc;
	int opt;
	int i;

	memset (&globals, 0x00, sizeof (globals) );
	while ( (opt = getopt (argc, argv, "vh") ) != -1) {
		switch (opt) {
			case 'v':
				show_version ();
				exit (0);
				break;
			case 'h':
				help ();
				exit (0);
				break;
			default:
				exit (1);
		}
	}

	/* get user's home directory */
	const char *homeDir = getenv ("HOME");
	if (!homeDir) {
		struct passwd* pwd = getpwuid (getuid () );
		if (pwd)
			homeDir = pwd->pw_dir;
	}
	strcpy (globals.home_dir, homeDir);
	sprintf (globals.app_dir, "%s/.%s", globals.home_dir, PACKAGE_NAME);
	sprintf (globals.connections_xml, "%s/connections.xml", globals.app_dir);
	sprintf (globals.log_file, "%s/lterm.log", globals.app_dir);
	sprintf (globals.profiles_file, "%s/profiles.xml", globals.app_dir);
	sprintf (globals.protocols_file, "%s/protocols.xml", globals.app_dir);
	sprintf (globals.conf_file, "%s/%s.conf", globals.app_dir, PACKAGE_NAME);
	globals.connected = 0;
	strcpy (globals.img_dir, IMGDIR);
	strcpy (globals.data_dir, PKGDATADIR);
	log_reset ();
	log_write ("Starting %s %s\n", PACKAGE, VERSION);
	log_write ("GTK version: %d.%d.%d\n", GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION);
	log_write ("libssh version %s\n", ssh_version (0) );
	log_write ("Desktop environment: %s\n", get_desktop_environment_name (get_desktop_environment () ) );
	log_debug ("globals.home_dir=%s\n", globals.home_dir);
	log_debug ("globals.img_dir=%s\n", globals.img_dir);
	log_debug ("globals.data_dir=%s\n", globals.data_dir);
	log_write ("Loading settings...\n");
	load_settings ();
	mkdir (globals.app_dir, S_IRWXU | S_IRWXG | S_IRWXO);
	log_write ("Loading profiles...\n");
	rc = load_profile (&g_profile, globals.profiles_file);
	if (rc != 0) {
		log_write ("Creating default profile...\n");
		profile_create_default (&g_profile);
	}
	ssh_list_init (&globals.ssh_list);
	log_write ("Initializing threads...\n");
	ssh_threads_set_callbacks (ssh_threads_get_pthread() );
	log_write ("Building gui...\n");
	start_gtk (argc, argv);
	// Register application. Needed for desktop notifications
	application = g_application_new ("lterm.application", G_APPLICATION_FLAGS_NONE);
	g_application_register (application, NULL, NULL);
	log_write ("Initializing iteration function requests\n");
	ifr_init ();
	log_write ("Initializing SFTP\n");
	ssh_init ();
	/* log on window */
	if (prefs.startup_show_connections)
		connection_log_on ();
	/* start main loop */
	globals.running = 1;
	log_write ("Starting main loop\n");
	while (globals.running) {
		lterm_iteration ();
	}
	log_write ("Saving connections...\n");
	save_connections_to_file_xml (globals.connections_xml);
	log_write ("Saving settings...\n");
	save_settings ();
	log_write ("Saving profiles...\n");
	save_profile (&g_profile, globals.profiles_file);
	g_object_unref (application);
	log_write ("End\n");
	return 0;
}

void
load_settings ()
{
	int rc;
	char last_package_version[16];
	rc = profile_load_string (globals.conf_file, "general", "package_version", last_package_version, "0.0.0");
	/* no profile found means this is the first installation */
	if (rc == PROFILE_FILE_NOT_FOUND) {
		strcpy (last_package_version, "100.100.100");
	}
	log_debug ("Actual version is %s, last version was %s\n", VERSION, last_package_version);
	/* load settings */
	/* emulation_list is not saved on exit, actually */
	//profile_load_string (globals.conf_file, "general", "emulation_list", prefs.emulation_list, "xterm:vt100:vt220:vt320:vt440");
	prefs.tabs_position = profile_load_int (globals.conf_file, "general", "tabs_position", GTK_POS_TOP);
	prefs.check_connections = profile_load_int (globals.conf_file, "general", "check_connections", 1);
	profile_load_string (globals.conf_file, "general", "warnings_color", prefs.warnings_color, "orange");
	profile_load_string (globals.conf_file, "general", "warnings_error_color", prefs.warnings_error_color, "red");
	profile_load_string (globals.conf_file, "general", "local_start_directory", prefs.local_start_directory, "");
	profile_load_string (globals.conf_file, "general", "font_fixed", prefs.font_fixed, DEFAULT_FIXED_FONT);
	profile_load_string (globals.conf_file, "general", "tempDir", prefs.tempDir, globals.app_dir/*"/tmp"*/);
	prefs.startup_show_connections = profile_load_int (globals.conf_file, "TERMINAL", "startup_show_connections", 0);
	prefs.startup_local_shell = switch_local ? 1 : profile_load_int (globals.conf_file, "TERMINAL", "startup_local_shell", 1);
	profile_load_string (globals.conf_file, "TERMINAL", "extra_word_chars", prefs.extra_word_chars, ":@-./_~?&=%+#");
	prefs.rows = profile_load_int (globals.conf_file, "TERMINAL", "rows", 80);
	prefs.columns = profile_load_int (globals.conf_file, "TERMINAL", "columns", 25);
	profile_load_string (globals.conf_file, "TERMINAL", "character_encoding", prefs.character_encoding, "");
	prefs.scrollback_lines = profile_load_int (globals.conf_file, "TERMINAL", "scrollback_lines", 512);
	prefs.scroll_on_keystroke = profile_load_int (globals.conf_file, "TERMINAL", "scroll_on_keystroke", 1);
	prefs.scroll_on_output = profile_load_int (globals.conf_file, "TERMINAL", "scroll_on_output", 1);
	prefs.mouse_autohide = profile_load_int (globals.conf_file, "MOUSE", "autohide", 1);
	prefs.mouse_copy_on_select = profile_load_int (globals.conf_file, "MOUSE", "copy_on_select", 0);
	prefs.mouse_paste_on_right_button = profile_load_int (globals.conf_file, "MOUSE", "paste_on_right_button", 0);
	/*prefs.x = profile_load_int (globals.conf_file, "GUI", "x", 0);
	prefs.y = profile_load_int (globals.conf_file, "GUI", "y", 0);*/
	prefs.w = profile_load_int (globals.conf_file, "GUI", "w", 640);
	prefs.h = profile_load_int (globals.conf_file, "GUI", "h", 480);
	prefs.maximize = profile_load_int (globals.conf_file, "GUI", "maximize", 0);
	prefs.toolbar = profile_load_int (globals.conf_file, "GUI", "toolbar", 1);
	prefs.statusbar = profile_load_int (globals.conf_file, "GUI", "statusbar", 1);
	prefs.fullscreen = profile_load_int (globals.conf_file, "GUI", "fullscreen", 0);
	prefs.tab_alerts = profile_load_int (globals.conf_file, "GUI", "tab_alerts", 1);
	profile_load_string (globals.conf_file, "GUI", "tab_status_changed_color", prefs.tab_status_changed_color, "blue");
	profile_load_string (globals.conf_file, "GUI", "tab_status_disconnected_color", prefs.tab_status_disconnected_color, "#707070");
	profile_load_string (globals.conf_file, "GUI", "tab_status_disconnected_alert_color", prefs.tab_status_disconnected_alert_color, "darkred");
}

void
save_settings ()
{
	int err;
	/* store the version of program witch saved this profile */
	profile_modify_string (PROFILE_SAVE, globals.conf_file, "general", "package_version", VERSION);
	profile_modify_int (PROFILE_SAVE, globals.conf_file, "general", "tabs_position", prefs.tabs_position);
	profile_modify_int (PROFILE_SAVE, globals.conf_file, "general", "check_connections", prefs.check_connections);
	profile_modify_string (PROFILE_SAVE, globals.conf_file, "general", "warnings_color", prefs.warnings_color);
	profile_modify_string (PROFILE_SAVE, globals.conf_file, "general", "local_start_directory", prefs.local_start_directory);
	profile_modify_string (PROFILE_SAVE, globals.conf_file, "general", "font_fixed", prefs.font_fixed);
	profile_modify_string (PROFILE_SAVE, globals.conf_file, "general", "tempDir", prefs.tempDir);
	profile_modify_int (PROFILE_SAVE, globals.conf_file, "TERMINAL", "startup_show_connections", prefs.startup_show_connections);
	profile_modify_int (PROFILE_SAVE, globals.conf_file, "TERMINAL", "startup_local_shell", prefs.startup_local_shell);
	profile_modify_int (PROFILE_SAVE, globals.conf_file, "TERMINAL", "scrollback_lines", prefs.scrollback_lines);
	profile_modify_int (PROFILE_SAVE, globals.conf_file, "TERMINAL", "scroll_on_keystroke", prefs.scroll_on_keystroke);
	profile_modify_int (PROFILE_SAVE, globals.conf_file, "TERMINAL", "scroll_on_output", prefs.scroll_on_output);
	profile_modify_int (PROFILE_SAVE, globals.conf_file, "TERMINAL", "rows", prefs.rows);
	profile_modify_int (PROFILE_SAVE, globals.conf_file, "TERMINAL", "columns", prefs.columns);
	profile_modify_int (PROFILE_SAVE, globals.conf_file, "MOUSE", "autohide", prefs.mouse_autohide);
	profile_modify_int (PROFILE_SAVE, globals.conf_file, "MOUSE", "copy_on_select", prefs.mouse_copy_on_select);
	profile_modify_int (PROFILE_SAVE, globals.conf_file, "MOUSE", "paste_on_right_button", prefs.mouse_paste_on_right_button);
	profile_modify_int (PROFILE_SAVE, globals.conf_file, "GUI", "maximize", prefs.maximize);
	profile_modify_int (PROFILE_SAVE, globals.conf_file, "GUI", "w", prefs.w);
	profile_modify_int (PROFILE_SAVE, globals.conf_file, "GUI", "h", prefs.h);
	profile_modify_int (PROFILE_SAVE, globals.conf_file, "GUI", "tab_alerts", prefs.tab_alerts);
}

void
get_version (char *version)
{
	strcpy (version, "");
#ifdef CREATIONDATE
	sprintf (version, "%s-%s", VERSION, CREATIONDATE);
#else
	sprintf (version, "%s", VERSION);
#endif
#ifdef SO
	strcat (version, " ");
	strcat (version, SO);
#endif
}


void
show_version ()
{
	char version[64];
	get_version (version);
	printf ("%s\n", version);
}

void
help ()
{
	char version[64];
	get_version (version);
	printf ("\n%s version %s\n", PACKAGE, version);
	printf (
	        "Usage :\n"
	        "	-v	show version\n"
	        "	-h	help\n"
	);
}

