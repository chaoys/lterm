
/*
  Header: main.h
  Global structures
*/

#ifndef _MAIN_H
#define _MAIN_H

#include "ssh.h"
#include "gtk/gtk.h"

#define log_debug(format, ...)

#include "config.h"

#define KEY "You may say I'm a dreamer"

#define HOME_PAGE "lterm.sourceforge.net"
#define HTTP_HOME_PAGE "http://lterm.sourceforge.net"

#define LOG_MSG printf

/* Version of configuration files */
#define CFG_VERSION 3
#define CFG_XML_VERSION 6

#define DEFAULT_FIXED_FONT "Monospace 9"

/*
  Struct: _globals
  Structure containing global data such as home and work directory, etc.
*/
struct _globals {
	int running;
	char home_dir[256];
	char app_dir[300];
	char img_dir[512];
	char data_dir[512];
	char connections_xml[512];    /* Server list file (xml format)*/
	char conf_file[512];
	char log_file[512];
	char profiles_file[512];
	char protocols_file[512];
	int connected;
	int original_font_size;
	char system_font[256];
	struct SSH_List ssh_list;
	char find_expr[256];
};

typedef struct _globals Globals;

/*
  Struct: _prefs
  Structure containing the user preferences
 */
struct _prefs {
	int startup_show_connections;
	int maximize;
	int x, y, w, h;               /* Window position and dimension */
	int rows;
	int columns;
	char extra_word_chars[256];
	char character_encoding[64];
	int scrollback_lines;
	int scroll_on_keystroke;
	int scroll_on_output;
	int mouse_autohide;
	int mouse_copy_on_select;
	int mouse_paste_on_right_button;
	int tabs_position;
	int tab_alerts;
	char tab_status_changed_color [32];
	char tab_status_disconnected_color [32];
	char tab_status_disconnected_alert_color [32];
	char font_fixed [128];

	int ssh_keepalive;
	int ssh_timeout;

	char tempDir[1024];
};

typedef struct _prefs Prefs;

#define PROT_FLAG_NO 0
#define PROT_FLAG_ASKUSER 1
#define PROT_FLAG_ASKPASSWORD 2
#define PROT_FLAG_DISCONNECTCLOSE 4
#define PROT_FLAG_MASK 255

struct Protocol {
	char command[256];
	char args[256];
	int port;
	unsigned int flags;
};

void lockSSH (const char *caller, gboolean flagLock);
void lterm_iteration ();
void log_reset ();
void log_write (const char *fmt, ...);
gboolean doGTKMainIteration ();
void timerStart (int seconds);
void timerStop ();
int timedOut ();
void threadRequestAlarm ();
void threadResetAlarm ();
void addIdleGTKMainIteration ();
void update_main_window_title ();

#endif
