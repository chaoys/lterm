
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
 * @file gui.c
 * @brief Graphic user interface functions
 */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <libssh/libssh.h>
#include <vte/vte.h>
#include <openssl/opensslv.h>
#include <openssl/md5.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include "main.h"
#include "connection.h"
#include "preferences.h"
#include "profile.h"
#include "gui.h"
#include "utils.h"
#include "terminal.h"
#include "connection_list.h"

#include <gdk/gdkx.h>

#define SHORTCUT_COPY "<shift><ctrl>C"
#define SHORTCUT_PASTE "<shift><ctrl>V"
#define SHORTCUT_FIND "<shift><ctrl>F"
#define SHORTCUT_FIND_NEXT "<shift><ctrl>G"
#define SHORTCUT_QUIT "<Alt>X"

extern Globals globals;
extern Prefs prefs;
extern struct Connection_List conn_list;
extern struct Profile g_profile;
extern struct GroupTree g_groups;

int ifr_index_first = 0;
int ifr_index_insert = 1;
struct Iteration_Function_Request ifr[ITERATION_MAX];

GtkWidget *main_window;
GtkWidget *hpaned;

GtkWidget *menubar, *main_toolbar;
GSimpleActionGroup *action_group;
GtkWidget *notebook;

GdkScreen *g_screen;

/* pointer to the list of open connections or NULL if there's no open connection */
GList *connection_tab_list;
struct ConnectionTab *p_current_connection_tab;

GtkActionGroup *profile_action_group;
int profile_menu_ui_id;

/* number of rows and columns before maximization */
int prev_rows;
int prev_columns;

int switch_tab_enabled = 1;

char *g_vte_selected_text = NULL;

// Cluster
enum { COLUMN_CLUSTER_TERM_SELECTED, COLUMN_CLUSTER_TERM_NAME, N_CLUSTER_COLUMNS };
GtkListStore *list_store_cluster;
typedef struct TabSelection {
	struct ConnectionTab *pTab;
	gint selected;
} STabSelection;
GArray *tabSelectionArray;

void test(void)
{
	printf("test\n");
}
GActionEntry main_menu_items[] = {
	{ "log_on", connection_log_on },
	{ "log_off", connection_log_off },
	{ "duplicate", connection_duplicate },
	{ "quit", application_quit },

	{ "copy", edit_copy },
	{ "paste", edit_paste },
	{ "copy_paste", edit_copy_and_paste },
	{ "find", edit_find },
	{ "findnext", terminal_find_next },
	{ "findprev", terminal_find_previous },
	{ "select_all", edit_select_all },
	{ "pref", show_preferences },

	{ "reset", terminal_reset },
	{ "detach_right", terminal_detach_right },
	{ "detach_down", terminal_detach_down },
	{ "attach_current", terminal_attach_current_to_main },
	{ "regroup_all", terminal_regroup_all },
	{ "send_cluster", terminal_cluster },

	{ "about", Info },
};

const gchar ui_main_desc[] =
        "<interface>"
		"<menu id='MainMenu'>"
        "    <submenu>"
		"      <attribute name='label'>Connection</attribute>"
		"      <section>"
        "        <item>"
		"          <attribute name='label'>Log on</attribute>"
		"          <attribute name='action'>lt.log_on</attribute>"
		"        </item>"
        "        <item>"
		"          <attribute name='label'>Log off</attribute>"
		"          <attribute name='action'>lt.log_off</attribute>"
		"        </item>"
        "        <item>"
		"          <attribute name='label'>Duplicate</attribute>"
		"          <attribute name='action'>lt.duplicate</attribute>"
		"        </item>"
		"      </section>"
		"      <section>"
		"        <item>"
		"          <attribute name='label'>Quit</attribute>"
		"          <attribute name='action'>lt.quit</attribute>"
		"        </item>"
		"      </section>"
        "    </submenu>"
        "    <submenu>"
		"      <attribute name='label'>Edit</attribute>"
		"      <section>"
		"        <item>"
		"          <attribute name='label'>Copy</attribute>"
		"          <attribute name='action'>lt.copy</attribute>"
		"        </item>"
		"        <item>"
		"          <attribute name='label'>Paste</attribute>"
		"          <attribute name='action'>lt.paste</attribute>"
		"        </item>"
		"        <item>"
		"          <attribute name='label'>Copy and paste</attribute>"
		"          <attribute name='action'>lt.copy_paste</attribute>"
		"        </item>"
		"      </section>"
		"      <section>"
		"        <item>"
		"          <attribute name='label'>Find</attribute>"
		"          <attribute name='action'>lt.find</attribute>"
		"        </item>"
		"        <item>"
		"          <attribute name='label'>FindNext</attribute>"
		"          <attribute name='action'>lt.findnext</attribute>"
		"        </item>"
		"        <item>"
		"          <attribute name='label'>FindPrevious</attribute>"
		"          <attribute name='action'>lt.findprev</attribute>"
		"        </item>"
		"      </section>"
		"      <section>"
		"        <item>"
		"          <attribute name='label'>Select all</attribute>"
		"          <attribute name='action'>lt.select_all</attribute>"
		"        </item>"
		"      </section>"
		"      <section>"
		"        <item>"
		"          <attribute name='label'>Preferences</attribute>"
		"          <attribute name='action'>lt.pref</attribute>"
		"        </item>"
		"      </section>"
        "    </submenu>"
        "    <submenu>"
		"      <attribute name='label'>Terminal</attribute>"
		"        <item>"
		"          <attribute name='label'>Reset</attribute>"
		"          <attribute name='action'>lt.reset</attribute>"
		"        </item>"
		"        <item>"
		"          <attribute name='label'>DetachRight</attribute>"
		"          <attribute name='action'>lt.detach_right</attribute>"
		"        </item>"
		"        <item>"
		"          <attribute name='label'>DetachDown</attribute>"
		"          <attribute name='action'>lt.detach_down</attribute>"
		"        </item>"
		"        <item>"
		"          <attribute name='label'>AttachCurrent</attribute>"
		"          <attribute name='action'>lt.attach_current</attribute>"
		"        </item>"
		"        <item>"
		"          <attribute name='label'>RegroupAll</attribute>"
		"          <attribute name='action'>lt.regroup_all</attribute>"
		"        </item>"
		"        <item>"
		"          <attribute name='label'>Cluster</attribute>"
		"          <attribute name='action'>lt.send_cluster</attribute>"
		"        </item>"
        "    </submenu>"
        "    <submenu>"
		"      <attribute name='label'>Help</attribute>"
		"        <item>"
		"          <attribute name='label'>About</attribute>"
		"          <attribute name='action'>lt.about</attribute>"
		"        </item>"
        "    </submenu>"
        "</menu>"
		"<interface>"
		;

const gchar *ui_popup_desc =
"<?xml version='1.0' encoding='UTF-8'?>"
"<!-- Generated with glade 3.20.0 -->"
"<interface>"
"<requires lib='gtk+' version='3.20'/>"
"<object class='GtkMenu' id='popup_menu'>"
"  <property name='visible'>True</property>"
"  <property name='can_focus'>False</property>"
"  <child>"
"    <object class='GtkMenuItem' id='pop1'>"
"      <property name='visible'>True</property>"
"      <property name='can_focus'>False</property>"
"      <property name='label' translatable='yes'>Test</property>"
"      <property name='use_underline'>True</property>"
"    </object>"
"  </child>"
"</object>"
"</interface>";

struct EncodingEntry {
	char name[64];
	char id[32];
};

struct EncodingEntry enc_array[] = {
	{ "UTF-8", "UTF-8" },
	{ "ISO-8859-1 (Latin 1, West European)", "ISO-8859-1" },
	{ "ISO-8859-2 (Latin 2, East European)", "ISO-8859-2" },
	{ "ISO-8859-3 (Latin 3, South European)", "ISO-8859-3" },
	{ "ISO-8859-4 (Latin 4, North European)", "ISO-8859-4" },
	{ "ISO-8859-5 (Cyrillic)", "ISO-8859-5" },
	{ "ISO-8859-6 (Arabic)", "ISO-8859-6" },
	{ "ISO-8859-7 (Greek)", "ISO-8859-7" },
	{ "ISO-8859-8 (Hebrew)", "ISO-8859-8" },
	{ "ISO-8859-9 (Turkish)", "ISO-8859-9" },
	{ "ISO-8859-10 (Nordic)", "ISO-8859-10" },
	{ "ISO-8859-11 (Thai)", "ISO-8859-11" },
	{ "ISO-8859-13 (Latin 7)", "ISO-8859-13" },
	{ "ISO-8859-14 (Latin 8)", "ISO-8859-14" },
	{ "ISO-8859-15 (Latin 9, West European with EURO)", "ISO-8859-15" },
	{ "GB2312 (Chinese)", "GB2312" },
	{ "BIG5 (Chinese)", "BIG5" },
	{ "Shift_JIS (Japanese)", "Shift_JIS" },
	{ "KSC (Korean)", "KSC" },
	{ "KOI8-R (Russian)", "KOI8-R" },
	{ "CP1251 (Russian)", "CP1251" },
	{ "EUC-JP (Japanese)", "EUC-JP" }
};

void
ifr_init (void)
{
	int i;
	for (i = 0; i < ITERATION_MAX; i++)
		memset (&ifr[i], 0, sizeof (struct Iteration_Function_Request) );
	ifr_index_first = 0;
	ifr_index_insert = 0;
}

void
ifr_add (int function_id, void *user_data)
{
	/* Check if all slots are busy */
	if (ifr[ifr_index_insert].id != 0)
		return;
	/* Avoid adjacent duplicates */
	if (ifr[ifr_index_first].id == function_id)
		return;
	log_debug ("ifr[%d] = %d\n", ifr_index_insert, function_id);
	ifr[ifr_index_insert].id = function_id;
	ifr[ifr_index_insert].user_data = user_data;
	ifr_index_insert = (ifr_index_insert + 1) % ITERATION_MAX;
}

int
ifr_get (struct Iteration_Function_Request *dest)
{
	if (ifr[ifr_index_first].id == 0)
		return (0);
	dest->id = ifr[ifr_index_first].id;
	dest->user_data = ifr[ifr_index_first].user_data;
	ifr[ifr_index_first].id = 0;
	ifr_index_first = (ifr_index_first + 1) % ITERATION_MAX;
	return (1);
}

void
msgbox_error (const char *fmt, ...)
{
	GtkWidget *dialog;
	va_list ap;
	char msg[2048];
	va_start (ap, fmt);
	vsprintf (msg, fmt, ap);
	va_end (ap);
	dialog = gtk_message_dialog_new (GTK_WINDOW (main_window), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, msg, 0);
	gtk_window_set_title (GTK_WINDOW (dialog), PACKAGE);
	gtk_dialog_run (GTK_DIALOG (dialog) );
	gtk_widget_destroy (dialog);
	log_write ("%s\n", msg);
}

void
msgbox_info (const char *fmt, ...)
{
	GtkWidget *dialog;
	va_list ap;
	char msg[1024];
	va_start (ap, fmt);
	vsprintf (msg, fmt, ap);
	va_end (ap);
	dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
	                                 GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, msg, 0);
	gtk_window_set_title (GTK_WINDOW (dialog), PACKAGE);
	gtk_dialog_run (GTK_DIALOG (dialog) );
	gtk_widget_destroy (dialog);
}

gint
msgbox_yes_no (const char *fmt, ...)
{
	GtkWidget *dialog;
	va_list ap;
	char msg[1024];
	va_start (ap, fmt);
	vsprintf (msg, fmt, ap);
	va_end (ap);
	dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
	                                 GTK_DIALOG_DESTROY_WITH_PARENT,
	                                 GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, msg, 0);
	gint result = gtk_dialog_run (GTK_DIALOG (dialog) );
	gtk_widget_destroy (dialog);
	return result;
}

/**
 * child_exit() - handler of SIGCHLD
 */
void
child_exit ()
{
	int pid, status;
	pid = wait (&status);
	if (status > 0)
		printf ("process %d terminated with code %d (%s)\n", pid, status, strerror (status) );
}

void
segv_handler (int signum)
{
	log_write("Critical error. Received signal %d\n", signum);
	msgbox_error ("Received signal %d.\n"
	              "Sorry, this is a critical error and the program will be killed.\n"
	              "Take a look on the website about what to do in cases like this.",
	              signum);
	signal (signum, SIG_DFL);
	kill (getpid(), signum);
}

void
tabInitConnection (SConnectionTab *pConn)
{
	tabSetConnectionStatus (pConn, TAB_CONN_STATUS_DISCONNECTED);
	pConn->auth_state = AUTH_STATE_NOT_LOGGED;
	pConn->auth_attempt = 0;
	pConn->changes_count = 0;
	pConn->flags = 0;
}

char *connectionStatusDesc[] = { "disconnected", "connecting", "connected" };

char *tabGetConnectionStatusDesc (int status)
{
	return (connectionStatusDesc[status]);
}

void tabSetConnectionStatus (SConnectionTab *pConn, int status)
{
	pConn->connectionStatus = status;
}

int tabGetConnectionStatus (SConnectionTab *pConn)
{
	return (pConn->connectionStatus);
}

int tabIsConnected (SConnectionTab *pConn)
{
	return (pConn->connectionStatus >= TAB_CONN_STATUS_CONNECTED);
}

void tabSetFlag (SConnectionTab *pConn, unsigned int bitmask)
{
	pConn->flags |= bitmask;
}

void tabResetFlag (SConnectionTab *pConn, unsigned int bitmask)
{
	pConn->flags &= ~bitmask;
}

unsigned int tabGetFlag (SConnectionTab *pConn, unsigned int bitmask)
{
	return (pConn->flags & bitmask);
}

void
set_title (char *user_s)
{
	char title[256];
	char appname[256];
	sprintf (appname, "%s %s", PACKAGE, VERSION);
	if (user_s)
		sprintf (title, "%s - %s", user_s, appname);
	else
		strcpy (title, appname);
	gtk_window_set_title (GTK_WINDOW (main_window), title);
}

/**
 * query_value() - open a dialog asking for a parameter value
 * @return length of value or -1 if user cancelled operation
 */
int
query_value (char *title, char *labeltext, char *default_value, char *buffer, int type)
{
	int ret;
	char imagefile[256];
	GtkWidget *dialog;
	GtkWidget *p_label;
	GtkWidget *user_entry;
	dialog = gtk_dialog_new_with_buttons (title, NULL, 0,
	                                      "_Cancel",
	                                      GTK_RESPONSE_CANCEL,
	                                      "_Ok",
	                                      GTK_RESPONSE_OK,
	                                      NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog) ) ), 10);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	user_entry = gtk_entry_new ();
	gtk_entry_set_max_length (GTK_ENTRY (user_entry), 512);
	gtk_entry_set_text (GTK_ENTRY (user_entry), default_value);
	gtk_entry_set_activates_default (GTK_ENTRY (user_entry), TRUE);
	gtk_entry_set_visibility (GTK_ENTRY (user_entry), type == QUERY_PASSWORD ? FALSE : TRUE);
	gtk_window_set_transient_for (GTK_WINDOW (GTK_DIALOG (dialog) ), GTK_WINDOW (main_window) );
	p_label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (p_label), labeltext);
	GtkWidget *main_hbox;
	GtkWidget *text_vbox;
	main_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
	text_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
	/* image */
	gint w_width, w_height;
	switch (type) {
		case QUERY_USER:
			strcpy (imagefile, "/user.png");
			break;
		case QUERY_PASSWORD:
			strcpy (imagefile, "/keys-64.png");
			break;
		case QUERY_FILE_NEW:
			strcpy (imagefile, "/file_new.png");
			break;
		case QUERY_FOLDER_NEW:
			strcpy (imagefile, "/folder_new.png");
			break;
		case QUERY_RENAME:
			strcpy (imagefile, "/rename.png");
			gtk_window_get_size (GTK_WINDOW (dialog), &w_width, &w_height);
			get_monitor_size(GTK_WINDOW(main_window), &w_width, NULL);
			gtk_widget_set_size_request (GTK_WIDGET (dialog), w_width / 1.8, w_height);
			break;
		default:
			strcpy (imagefile, "");
			break;
	}
	if (imagefile[0]) {
		gtk_box_pack_start (GTK_BOX (main_hbox), gtk_image_new_from_file (g_strconcat (globals.img_dir, imagefile, NULL) ), FALSE, TRUE, 2);
	} else
		gtk_container_set_border_width (GTK_CONTAINER (main_hbox), 0);
	/* text and field */
	gtk_box_pack_start (GTK_BOX (text_vbox), p_label, FALSE, TRUE, 5);
	gtk_box_pack_start (GTK_BOX (text_vbox), user_entry, FALSE, TRUE, 5);
	gtk_box_pack_start (GTK_BOX (main_hbox), text_vbox, TRUE, TRUE, 5);
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog) ) ), main_hbox);
	gtk_widget_show_all (dialog);
	gtk_widget_grab_focus (user_entry);
	/* Start the dialog window */
	gint result = gtk_dialog_run (GTK_DIALOG (dialog) );
	strcpy (buffer, "");
	if (result == GTK_RESPONSE_OK) {
		strcpy (buffer, gtk_entry_get_text (GTK_ENTRY (user_entry) ) );
		ret = strlen (buffer);
	} else
		ret = -1;
	gtk_widget_destroy (dialog);
	return (ret);
}

/**
 * expand_args() - expands command line parameters
 * @param[in] p_conn pointer to chosen connection (ip, port)
 * @param[in] args string to be expanded
 * @param[in] prefix a static string to put before expended
 * @param[out] dest string buffer receiving the expanded string
 * @return 0 if ok, 1 otherwise
 */
int
expand_args (struct Connection *p_conn, char *args, char *prefix, char *dest)
{
	int i, c, i_dest;
	int go_on;
	char expanded[256];
	char title[256];
	char label[512];
	i = 0;
	go_on = 1;
	strcpy (dest, prefix != NULL ? prefix : "");
	if (prefix)
		strcat (dest, " ");
	i_dest = strlen (dest);
	while (go_on > 0 && i < strlen (args) ) {
		c = args[i];
		if (c == '%') {
			i ++;
			c = args[i];
			switch (c) {
				/* host */
				case 'h':
					strcpy (expanded, p_conn->host);
					break;
				/* port */
				case 'p':
					sprintf (expanded, "%d", p_conn->port);
					break;
				/* user */
				case 'u':
					if (p_conn->auth_mode == CONN_AUTH_MODE_SAVE || p_conn->user[0]) {
						strcpy (expanded, p_conn->user);
					} else {
						strcpy (title, "Log on");
						sprintf (label, ("Enter user for <b>%s</b>:"), p_conn->name);
						go_on = query_value (title, label, p_conn->last_user, expanded, QUERY_USER);
						strcpy (p_conn->user, expanded);
						//p_conn->auth_flags |= AUTH_STEP_USER;
					}
					rtrim (p_conn->user);
					if (p_conn->user[0] != 0) {
						strcpy (p_conn->last_user, p_conn->user);
						if (conn_update_last_user (p_conn->name, p_conn->last_user) )
							printf ("unable to update last user '%s' for connection %s\n", p_conn->user, p_conn->name);
					}
					break;
				/* password */
				case 'P':
					if (p_conn->auth_mode == CONN_AUTH_MODE_SAVE || p_conn->password[0]) {
						strcpy (expanded, p_conn->password);
					} else {
						strcpy (title, "Log on");
						sprintf (label, ("Enter password for <b>%s@%s</b>:"), p_conn->user, p_conn->name);
						go_on = query_value (title, label, "", expanded, QUERY_PASSWORD);
						strcpy (p_conn->password, expanded);
					}
					break;
				case '%':
					strcpy (expanded, "%");
					break;
				default:
					strcpy (expanded, "");
					break;
			}
			strcat (dest, expanded);
			i_dest += strlen (expanded);
		} else {
			dest[i_dest] = c;
			dest[i_dest + 1] = 0;
			i_dest ++;
		}
		i++;
	}
	return (go_on > 0 ? 0 : 1);
}

int
show_login_mask (struct ConnectionTab *p_conn_tab, struct SSH_Auth_Data *p_auth)
{
	GtkWidget *dialog;
	GtkBuilder *builder;
	GError *error = NULL;
	char ui[1024], image_auth_filename[1024];
	int result, rc = 0;
	builder = gtk_builder_new ();
	sprintf (ui, "%s/login.glade", globals.data_dir);
	sprintf (image_auth_filename, "%s/keys-64.png", globals.img_dir);
	if (gtk_builder_add_from_file (builder, ui, &error) == 0) {
		msgbox_error ("Can't load user interface file:\n%s", error->message);
		g_object_unref (G_OBJECT (builder) );
		return (1);
	}
	GtkWidget *hbox_main = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_main") );
	GtkWidget *image_auth = GTK_WIDGET (gtk_builder_get_object (builder, "image_auth") );
	GtkWidget *label_name = GTK_WIDGET (gtk_builder_get_object (builder, "label_name") );
	GtkWidget *label_ip = GTK_WIDGET (gtk_builder_get_object (builder, "label_ip") );
	GtkWidget *entry_user = GTK_WIDGET (gtk_builder_get_object (builder, "entry_user") );
	GtkWidget *entry_password = GTK_WIDGET (gtk_builder_get_object (builder, "entry_password") );
	gtk_label_set_text (GTK_LABEL (label_name), p_conn_tab->connection.name);
	gtk_label_set_text (GTK_LABEL (label_ip), p_conn_tab->connection.host);
	gtk_image_set_from_file (GTK_IMAGE (image_auth), image_auth_filename);
	if (p_conn_tab->connection.last_user[0])
		gtk_entry_set_text (GTK_ENTRY (entry_user), p_conn_tab->connection.last_user);
	/* Create dialog */
	dialog = gtk_dialog_new_with_buttons
	         (("Authentication"), NULL,
	          GTK_DIALOG_MODAL,
	          "_Cancel", GTK_RESPONSE_CANCEL,
	          "_Ok", GTK_RESPONSE_OK,
	          NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_entry_set_activates_default (GTK_ENTRY (entry_user), TRUE);
	gtk_entry_set_activates_default (GTK_ENTRY (entry_password), TRUE);
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_transient_for (GTK_WINDOW (GTK_DIALOG (dialog) ), GTK_WINDOW (main_window) );
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog) ) ), hbox_main);
	gtk_widget_show_all (gtk_dialog_get_content_area (GTK_DIALOG (dialog) ) );
	if (p_auth->mode == CONN_AUTH_MODE_KEY) {
		GtkWidget *label_password = GTK_WIDGET (gtk_builder_get_object (builder, "label_password") );
		gtk_widget_hide (GTK_WIDGET (entry_password) );
		gtk_widget_hide (GTK_WIDGET (label_password) );
	}
	result = gtk_dialog_run (GTK_DIALOG (dialog) );
	if (result == GTK_RESPONSE_OK) {
		lt_ssh_init (&p_conn_tab->ssh_info);
		strcpy (p_auth->user, gtk_entry_get_text (GTK_ENTRY (entry_user) ) );
		strcpy (p_auth->password, gtk_entry_get_text (GTK_ENTRY (entry_password) ) );
		strcpy (p_auth->host, p_conn_tab->connection.host);
		p_auth->port = p_conn_tab->connection.port;
		log_debug ("got user input: %s/%s\n", p_auth->user, p_auth->password);
		rc = 0;
	} else {
		rc = -1;
	}
	gtk_widget_destroy (dialog);
	g_object_unref (G_OBJECT (builder) );
	while (gtk_events_pending () )
		gtk_main_iteration ();
	return (rc);
}

int
connection_tab_count (void)
{
	int n = 0;
	GList *item;
	item = g_list_first (connection_tab_list);
	while (item) {
		if (tabIsConnected ( (struct ConnectionTab *) item->data))
			n ++;
		item = g_list_next (item);
	}
	return (n);
}

struct ConnectionTab *
get_current_connection_tab ()
{
	return (p_current_connection_tab);
}

struct ConnectionTab *
get_connection_tab_from_child (GtkWidget *child)
{
	int i;
	GList *item;
	struct ConnectionTab *p_ct = NULL;
	for (i = 0; i < g_list_length (connection_tab_list); i++) {
		item = g_list_nth (connection_tab_list, i);
		p_ct = (struct ConnectionTab *) item->data;
		if (p_ct->hbox_terminal == child)
			break;;
	}
	return (p_ct);
}

void
connection_tab_close (struct ConnectionTab *p_ct)
{
	int page, retcode, can_close;
	char prompt[512];
	if (tabIsConnected (p_ct) ) {
		sprintf (prompt, ("Close connection to %s?"), p_ct->connection.name);
		retcode = msgbox_yes_no (prompt);
		if (retcode == GTK_RESPONSE_YES)
			can_close = 1;
		else
			can_close = 0;
	} else
		can_close = 1;
	if (can_close) {
		// Regroup this tab to adjust the view
		if (p_ct->notebook != notebook)
			terminal_attach_to_main (p_ct);
		if (tabIsConnected (p_ct) ) {
				lt_ssh_disconnect (&p_ct->ssh_info);
		}
		page = gtk_notebook_page_num (GTK_NOTEBOOK (p_ct->notebook), p_ct->hbox_terminal);
		log_write ("page = %d\n", page);
		if (page >= 0) {
			log_write ("Removing page %d %s\n", page, p_ct->connection.name);
			gtk_notebook_remove_page (GTK_NOTEBOOK (p_ct->notebook), page);
			connection_tab_list = g_list_remove (connection_tab_list, p_ct);
			if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook) ) == 0)
				p_current_connection_tab = NULL;
		}
	}
	update_screen_info ();
}

void
close_button_clicked_cb (GtkButton *button, gpointer user_data)
{
	struct ConnectionTab *p_ct;
	p_ct = (struct ConnectionTab *) user_data;
	connection_tab_close (p_ct);
}

struct ConnectionTab *
connection_tab_new ()
{
	struct ConnectionTab *connection_tab;
	connection_tab = g_new0 (struct ConnectionTab, 1);
	connection_tab_list = g_list_append (connection_tab_list, connection_tab);
	connection_tab->vte = vte_terminal_new ();
	g_signal_connect (connection_tab->vte, "child-exited", G_CALLBACK (child_exited_cb), connection_tab);
	g_signal_connect (connection_tab->vte, "eof", G_CALLBACK (eof_cb), connection_tab);
	g_signal_connect (connection_tab->vte, "increase-font-size", G_CALLBACK (increase_font_size_cb), NULL);
	g_signal_connect (connection_tab->vte, "decrease-font-size", G_CALLBACK (decrease_font_size_cb), NULL);
	g_signal_connect (connection_tab->vte, "button-press-event", G_CALLBACK (button_press_event_cb), connection_tab->vte);
	g_signal_connect (connection_tab->vte, "window-title-changed", G_CALLBACK (window_title_changed_cb), connection_tab);
	g_signal_connect (connection_tab->vte, "selection-changed", G_CALLBACK (selection_changed_cb), connection_tab);
	g_signal_connect (connection_tab->vte, "grab-focus", G_CALLBACK (terminal_focus_cb), connection_tab);
	tabInitConnection (connection_tab);
	memset (&connection_tab->connection, 0, sizeof (struct Connection) );
	return (connection_tab);
}

void
connection_tab_add (struct ConnectionTab *connection_tab)
{
	GtkWidget *tab_label;
	GtkWidget *close_button;
	PangoFontDescription *font_desc;
	gint new_pagenum;
	connection_tab->hbox_terminal = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0); /* for vte and scrolbar */
	connection_tab->scrollbar = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL,
	                            gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (connection_tab->vte) ) );
	gtk_box_pack_start (GTK_BOX (connection_tab->hbox_terminal), connection_tab->vte, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (connection_tab->hbox_terminal), connection_tab->scrollbar, FALSE, FALSE, 0);
	gtk_widget_show_all (connection_tab->hbox_terminal);
	tab_label = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_set_border_width (GTK_CONTAINER (tab_label), 0);
	gtk_box_set_spacing (GTK_BOX (tab_label), 8);
	GtkWidget *image_type;
	image_type = gtk_image_new_from_icon_name ("network-workgroup", GTK_ICON_SIZE_MENU);
	connection_tab->label = gtk_label_new (connection_tab->connection.name);
	close_button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);
	gtk_container_set_border_width (GTK_CONTAINER (close_button), 0);
	GtkWidget *image = gtk_image_new_from_icon_name ("window-close", GTK_ICON_SIZE_MENU);
	gtk_container_add (GTK_CONTAINER (close_button), image);
	g_signal_connect (close_button, "clicked", G_CALLBACK (close_button_clicked_cb), connection_tab);
	gtk_box_pack_end (GTK_BOX (tab_label), close_button, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (tab_label), connection_tab->label, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (tab_label), image_type, FALSE, FALSE, 0);
	gtk_widget_show_all (tab_label);
	new_pagenum = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook) ) + 1;
	gtk_notebook_insert_page_menu (GTK_NOTEBOOK (notebook), connection_tab->hbox_terminal, tab_label, gtk_label_new (connection_tab->connection.name), new_pagenum);
	gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (notebook), connection_tab->hbox_terminal, TRUE);
	gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (notebook), connection_tab->hbox_terminal, FALSE);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), new_pagenum);
	connection_tab->notebook = notebook;
	/* Store original font */
	font_desc = pango_font_description_copy (vte_terminal_get_font (VTE_TERMINAL (connection_tab->vte) ) );
	globals.original_font_size = pango_font_description_get_size (font_desc) / PANGO_SCALE;
	strcpy (globals.system_font, pango_font_description_to_string (font_desc) );
	pango_font_description_free (font_desc);
	apply_preferences (connection_tab->vte);
	apply_profile (connection_tab);
	gtk_widget_grab_focus (connection_tab->vte);
}

/**
 * refreshTabStatus() - Changes tab label depending on current status
 */
void
refreshTabStatus (SConnectionTab *pTab)
{
	char *l_text;
	char l_text_new[256];
	if (!prefs.tab_alerts)
		return;
	if (!GTK_IS_LABEL (pTab->label) )
		return;
	l_text = (char *) gtk_label_get_text (GTK_LABEL (pTab->label) );
	sprintf (l_text_new, "%s", l_text);
	if (tabIsConnected (pTab) ) {
		if (tabGetFlag (pTab, TAB_CHANGED) ) {
			if (pTab != p_current_connection_tab)
				sprintf (l_text_new, "<span color=\"%s\">%s</span>", prefs.tab_status_changed_color, l_text);
			else
				sprintf (l_text_new, "%s", l_text);
			tabResetFlag (pTab, TAB_CHANGED);
		}
	} else {
		if (pTab == p_current_connection_tab)
			sprintf (l_text_new, "<span color=\"%s\">%s</span>", prefs.tab_status_disconnected_color, l_text);
		else
			sprintf (l_text_new, "<span color=\"%s\">%s</span>", prefs.tab_status_disconnected_alert_color, l_text);
	}
	gtk_label_set_markup (GTK_LABEL (pTab->label), l_text_new);
}

int
connection_tab_getcwd (struct ConnectionTab *p_ct, char *directory)
{
	char buffer[1024];
	char window_title[1024];
	char *pc;
	if (p_ct == 0)
		return 1;
	strcpy (directory, "");
	if (p_ct->vte == 0)
		return 1;
	if (vte_terminal_get_window_title (VTE_TERMINAL (p_ct->vte) ) )
		strcpy (window_title, vte_terminal_get_window_title (VTE_TERMINAL (p_ct->vte) ) );
	else
		return 1;
	pc = (char *) strchr (window_title, ':');
	if (pc) {
		pc ++;
		strcpy (buffer, pc);
	}
	strcpy (directory, buffer);
	return 0;
}

void
connection_log_on_param (struct Connection *p_conn)
{
	int retcode = 0;
	struct ConnectionTab *p_connection_tab;
	p_connection_tab = connection_tab_new ();
	if (p_conn) {
		connection_copy (&p_connection_tab->connection, p_conn);
		p_connection_tab->auth_attempt = 0;
		tabResetFlag (p_connection_tab, TAB_LOGGED);
		log_debug ("connection '%s' log on with user '%s'\n", p_connection_tab->connection.name, p_connection_tab->connection.user);
	} else
		retcode = choose_manage_connection (&p_connection_tab->connection);
	if (retcode == 0) {
		if (p_connection_tab->connection.auth_mode == CONN_AUTH_MODE_SAVE && p_connection_tab->connection.auth_user[0])
			strcpy (p_connection_tab->connection.user, p_connection_tab->connection.auth_user);
		if (p_connection_tab->connection.auth_mode == CONN_AUTH_MODE_SAVE && p_connection_tab->connection.auth_password[0])
			strcpy (p_connection_tab->connection.password, p_connection_tab->connection.auth_password);
		/* Add the new tab */
		connection_tab_add (p_connection_tab);
		p_current_connection_tab = p_connection_tab;
		refreshTabStatus (p_current_connection_tab);
		log_write ("Log on...\n");
		log_on (p_connection_tab);
		refreshTabStatus (p_current_connection_tab);
	}
	update_screen_info ();
}

/**
 * connection_log_on() - this is the function called by menu item
 */
void
connection_log_on ()
{
	connection_log_on_param (NULL);
}

void
connection_log_off ()
{
	if (!p_current_connection_tab)
		return;
	if (tabIsConnected (p_current_connection_tab) ) {
		kill (p_current_connection_tab->pid, SIGTERM);
		lt_ssh_disconnect (&p_current_connection_tab->ssh_info);
		log_write ("Terminal closed\n");
		tabInitConnection (p_current_connection_tab);
	}
	update_screen_info ();
}

void
connection_duplicate ()
{
	char directory[1024];
	if (!p_current_connection_tab)
		return;
	connection_tab_getcwd (p_current_connection_tab, directory);
	/* force autentication */
	connection_log_on_param (&p_current_connection_tab->connection);
}

void
connection_close_tab ()
{
	if (!p_current_connection_tab)
		return;
	connection_tab_close (p_current_connection_tab);
}

void
application_quit ()
{
	int retcode;
	int can_quit = 1;
	int n;
	char message[1024];
	n = connection_tab_count ();
	if (n) {
		sprintf (message, ("There are %d active terminal/s.\nExit anyway?"), n);
		retcode = msgbox_yes_no (message);
		if (retcode == GTK_RESPONSE_YES)
			can_quit = 1;
		else
			can_quit = 0;
	}
	if (can_quit) {
		globals.running = 0;
	}
}

gchar *
utils_escape_underscores (const gchar* text, gssize length)
{
	GString *str;
	const gchar *p;
	const gchar *end;
	g_return_val_if_fail (text != NULL, NULL);
	if (length < 0)
		length = strlen (text);
	str = g_string_sized_new (length);
	p = text;
	end = text + length;
	while (p != end) {
		const gchar *next;
		next = g_utf8_next_char (p);
		switch (*p) {
			case '_':
				g_string_append (str, "__");
				break;
			default:
				g_string_append_len (str, p, next - p);
				break;
		}
		p = next;
	}
	return g_string_free (str, FALSE);
}

GtkWidget *
_get_active_widget()
{
	GtkWindow *active;
	GList *list = gtk_window_list_toplevels ();
	GList *item;
	item = g_list_first (list);
	while (item) {
		active = GTK_WINDOW (item->data);
		if (gtk_window_is_active (active) ) {
			GtkWidget *w = gtk_window_get_focus (active);
			return (w);
		}
		item = g_list_next (item);
	}
	return (NULL);
}

static inline void terminal_copy_to_clipboard(VteTerminal *vte)
{
#if VTE_CHECK_VERSION(0, 50, 0)
	vte_terminal_copy_clipboard_format (vte, VTE_FORMAT_TEXT);
#else
	vte_terminal_copy_clipboard(vte);
#endif
}

void
edit_copy ()
{
	int done = 0;
	/* Get the active widget */
	GtkWidget *w = _get_active_widget ();
	if (p_current_connection_tab) {
		/* Check if the terminal has the focus or the current widget is null (popup menu) */
		if (gtk_widget_has_focus (p_current_connection_tab->vte) || w == NULL) {
			terminal_copy_to_clipboard(VTE_TERMINAL (p_current_connection_tab->vte));
			done = 1;
		}
	}
	/* If we did nothing with the terminal then do it with the current widget */
	if (!done) {
		if (w)
			g_signal_emit_by_name (w, "copy-clipboard", NULL);
	}
}

void
edit_paste ()
{
	int done = 0;
	/* Get the active widget */
	GtkWidget *w = _get_active_widget ();
	/**
	 *  If terminal has the focus or the current widget is null (popup menu), then paste clipboard into it.
	 *  Else paste into the current active widget.
	 *  This is mainly needed on Mac OS X where paste is always Command+V
	 */
	if (p_current_connection_tab) {
		if (gtk_widget_has_focus (p_current_connection_tab->vte) || w == NULL) {
			vte_terminal_paste_clipboard (VTE_TERMINAL (p_current_connection_tab->vte) );
			done = 1;
		}
	}
	if (!done) {
		if (w)
			g_signal_emit_by_name (w, "paste-clipboard", NULL);
		/* See: http://stackoverflow.com/questions/10508810/find-out-which-gtk-widget-has-the-current-selection */
	}
}

void
edit_copy_and_paste ()
{
	if (!p_current_connection_tab)
		return;
	edit_copy ();
	edit_paste ();
}

void
edit_find ()
{
	GtkWidget *dialog;
	GtkBuilder *builder;
	GError *error = NULL;
	char ui[1024];
	int result, rc = 0;
	if (p_current_connection_tab == NULL)
		return;
	builder = gtk_builder_new ();
	sprintf (ui, "%s/find.glade", globals.data_dir);
	if (gtk_builder_add_from_file (builder, ui, &error) == 0) {
		msgbox_error ("Can't load user interface file:\n%s", error->message);
		g_object_unref (G_OBJECT (builder) );
		return;
	}
	GtkWidget *vbox_main = GTK_WIDGET (gtk_builder_get_object (builder, "vbox_main") );
	GtkWidget *entry_expr = GTK_WIDGET (gtk_builder_get_object (builder, "entry_expr") );
	if (globals.find_expr[0])
		gtk_entry_set_text (GTK_ENTRY (entry_expr), globals.find_expr);
	/* Create dialog */
	dialog = gtk_dialog_new_with_buttons
	         (("Find"), NULL,
	          GTK_DIALOG_MODAL,
	          "_Cancel", GTK_RESPONSE_CANCEL,
	          "_Ok", GTK_RESPONSE_OK,
	          NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_entry_set_activates_default (GTK_ENTRY (entry_expr), TRUE);
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_transient_for (GTK_WINDOW (GTK_DIALOG (dialog) ), GTK_WINDOW (main_window) );
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog) ) ), vbox_main);
	gtk_widget_show_all (gtk_dialog_get_content_area (GTK_DIALOG (dialog) ) );
	result = gtk_dialog_run (GTK_DIALOG (dialog) );
	if (result == GTK_RESPONSE_OK) {
		strcpy (globals.find_expr, gtk_entry_get_text (GTK_ENTRY (entry_expr) ) );
		terminal_set_search_expr (globals.find_expr);
		rc = 0;
	} else {
		rc = -1;
	}
	gtk_widget_destroy (dialog);
	g_object_unref (G_OBJECT (builder) );
	if (rc == 0)
		terminal_find_next ();
}

void
edit_select_all ()
{
	if (!p_current_connection_tab)
		return;
	vte_terminal_select_all (VTE_TERMINAL (p_current_connection_tab->vte) );
}

void
terminal_reset ()
{
	if (!p_current_connection_tab)
		return;
	vte_terminal_reset (VTE_TERMINAL (p_current_connection_tab->vte), TRUE, FALSE);
}

void
moveTab (struct ConnectionTab *connTab, GtkWidget *child, GtkWidget *notebookFrom, GtkWidget *notebookTo)
{
	// Detach tab label
	GtkWidget *tab_label = gtk_notebook_get_tab_label (GTK_NOTEBOOK (notebookFrom), child);
	g_object_ref (tab_label);
	gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (tab_label) ), tab_label);
	// Move terminal to the new notebook
	g_object_ref (child);
	gtk_container_remove (GTK_CONTAINER (notebookFrom), child);
	gtk_notebook_append_page_menu (GTK_NOTEBOOK (notebookTo), child, tab_label, gtk_label_new (p_current_connection_tab->connection.name) );
	connTab->notebook = notebookTo;
}

void
terminal_detach (GtkOrientation orientation)
{
	struct ConnectionTab *currentTab;
	/* if (!p_current_connection_tab)
	   return;*/
	if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook) ) < 2)
		return;
	// Get the current connection tab in the main notebook
	GtkWidget *child = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook) ) );
	//GtkWidget *child = currentTab->hbox_terminal;
	currentTab = get_connection_tab_from_child (child);
	log_write ("Detaching %s\n", currentTab->connection.name);
	GtkWidget *parent; // Main notebook parent
	GtkWidget *hpaned_split;
	// Detaching a tab causes switching to another tab, so disable switch management
	switch_tab_enabled = FALSE;
	// Create a new paned window
	hpaned_split = gtk_paned_new (orientation);
	// Get the notebook parent
	parent = gtk_widget_get_parent (notebook);
	// Re-parent main notebook
	g_object_ref (notebook);
	gtk_container_remove (GTK_CONTAINER (parent), notebook);
	gtk_paned_add1 (GTK_PANED (hpaned_split), notebook);
	// Create a new notebook
	GtkWidget *notebook_new = gtk_notebook_new ();
	gtk_paned_add2 (GTK_PANED (hpaned_split), notebook_new);
	// Move connection tab
	moveTab (currentTab, child, notebook, notebook_new);
	gtk_widget_show_all (hpaned_split);
	// Add the new paned window to the main one
	if (parent == hpaned)
		gtk_paned_add2 (GTK_PANED (hpaned), hpaned_split); // First division
	else
		gtk_paned_add1 (GTK_PANED (parent), hpaned_split);
	// Do an iteration, otherwise allocation is not correctly set
	lterm_iteration ();
	GtkAllocation allocation;
	gtk_widget_get_allocation (hpaned_split, &allocation);
	gtk_paned_set_position (GTK_PANED (hpaned_split),
	                        orientation == GTK_ORIENTATION_HORIZONTAL ? allocation.width / 2 : allocation.height / 2);
	switch_tab_enabled = TRUE;
}

void
terminal_detach_right()
{
	terminal_detach (GTK_ORIENTATION_HORIZONTAL);
}

void
terminal_detach_down()
{
	terminal_detach (GTK_ORIENTATION_VERTICAL);
}

void
terminal_attach_to_main (struct ConnectionTab *connectionTab)
{
	if (!connectionTab)
		return;
	GtkWidget *notebookCurrent = connectionTab->notebook;
	if (notebookCurrent == notebook) {
		return;
	}
	log_write ("Attaching %s\n", vte_terminal_get_window_title (VTE_TERMINAL (connectionTab->vte) ) );
	// Get the widget to be moved (surely it's the second child)
	GtkWidget *vteMove = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebookCurrent), gtk_notebook_get_current_page (GTK_NOTEBOOK (notebookCurrent) ) );
	// Get the widget than will remain (surely it's the first child)
	GtkWidget *parent = gtk_widget_get_parent (notebookCurrent);
	GtkWidget *childRemain = gtk_paned_get_child1 (GTK_PANED (parent) );
	// Move connection tab
	moveTab (connectionTab, vteMove, notebookCurrent, notebook);
	// Re-parent childRemain and destroy the paned
	g_object_ref (childRemain);
	gtk_container_remove (GTK_CONTAINER (parent), childRemain);
	gtk_widget_destroy (notebookCurrent);
	if (parent == hpaned)
		gtk_paned_add2 (GTK_PANED (hpaned), childRemain);
	else
		gtk_paned_add1 (GTK_PANED (parent), childRemain);
	gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (notebook), vteMove, TRUE);
}

void
terminal_attach_current_to_main ()
{
	if (!p_current_connection_tab)
		return;
	terminal_attach_to_main (p_current_connection_tab);
}

void
terminal_regroup_all ()
{
	int n = 0;
	struct ConnectionTab *cTab;
	GList *item;
	if (!p_current_connection_tab)
		return;
	item = g_list_first (connection_tab_list);
	while (item) {
		cTab = (struct ConnectionTab *) item->data;
		if (cTab->notebook != notebook) {
			terminal_attach_to_main (cTab);
			n ++;
		}
		item = g_list_next (item);
	}
	log_write ("Regrouped %d tab/s\n", n);
}

gboolean
cluster_get_selected (int i)
{
	STabSelection *selectedTab = &g_array_index (tabSelectionArray, STabSelection, i);
	return (selectedTab->selected);
}

void
cluster_set_selected (int i, gboolean value)
{
	GtkTreeModel *model = GTK_TREE_MODEL (list_store_cluster);
	GtkTreeIter iter;
	GtkTreePath *path;
	gchar path_str[64];
	sprintf (path_str, "%d", i);
	path = gtk_tree_path_new_from_string (path_str);
	gtk_tree_model_get_iter (model, &iter, path);
	STabSelection *selectedTab = &g_array_index (tabSelectionArray, STabSelection, i);
	selectedTab->selected = value;
	log_debug ("send-cluster: %s %d\n", selectedTab->pTab->connection.name, selectedTab->selected);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter, COLUMN_CLUSTER_TERM_SELECTED, selectedTab->selected, -1);
}

void
cluster_select_all_cb (GtkButton *button, gpointer user_data)
{
	int i;
	for (i = 0; i < tabSelectionArray->len; i++)
		cluster_set_selected (i, TRUE);
}

void
cluster_deselect_all_cb (GtkButton *button, gpointer user_data)
{
	int i;
	for (i = 0; i < tabSelectionArray->len; i++)
		cluster_set_selected (i, FALSE);
}

void
cluster_invert_selection_cb (GtkButton *button, gpointer user_data)
{
	int i;
	for (i = 0; i < tabSelectionArray->len; i++)
		cluster_set_selected (i, !cluster_get_selected (i) );
}

void
terminal_toggled_cb (GtkCellRendererToggle *cell_renderer, gchar *path_str, gpointer data)
{
	cluster_set_selected (atoi (path_str), !cluster_get_selected (atoi (path_str) ) );
}

/**
 * terminal_cluster()
 * Send the same command to the selected tabs
 */
void
terminal_cluster ()
{
	GtkBuilder *builder;
	GError *error = NULL;
	GtkWidget *dialog;
	char ui[1024];
	if (g_list_length (connection_tab_list) == 0) {
		log_write ("No tabs for cluster command\n");
		return;
	}
	builder = gtk_builder_new ();
	sprintf (ui, "%s/cluster.glade", globals.data_dir);
	if (gtk_builder_add_from_file (builder, ui, &error) == 0) {
		msgbox_error ("Can't load user interface file:\n%s", error->message);
		g_object_unref (G_OBJECT (builder) );
		return;
	}
	/* Create dialog */
	dialog = gtk_dialog_new_with_buttons
	         (("Cluster"), NULL,
	          GTK_DIALOG_MODAL,
	          "_Cancel", GTK_RESPONSE_CANCEL,
	          "_Apply", GTK_RESPONSE_OK,
	          NULL);
	GtkWidget *vbox = GTK_WIDGET (gtk_builder_get_object (builder, "vbox_cluster") );
	/* Terminal list */
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GtkWidget *tree_view = gtk_tree_view_new ();
	list_store_cluster = gtk_list_store_new (N_CLUSTER_COLUMNS, G_TYPE_BOOLEAN, G_TYPE_STRING);
	/* Selected */
	GtkCellRenderer/*Toggle*/ *toggle_cell = gtk_cell_renderer_toggle_new ();
	g_signal_connect (toggle_cell, "toggled", G_CALLBACK (terminal_toggled_cb), GTK_TREE_MODEL (list_store_cluster) );
	column = gtk_tree_view_column_new_with_attributes (("Enabled"), toggle_cell, "active", COLUMN_CLUSTER_TERM_SELECTED, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), GTK_TREE_VIEW_COLUMN (column) );
	/* Name */
	cell = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (("Tab"), cell, "text", COLUMN_CLUSTER_TERM_NAME, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), GTK_TREE_VIEW_COLUMN (column) );
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), TRUE);
	// Populate list
	GtkTreeIter iter;
	GList *item;
	struct ConnectionTab *p_ct;
	int i = 0;
	gtk_list_store_clear (list_store_cluster);
	// Create an array of integers for toggle terminals
	tabSelectionArray = g_array_new (FALSE, TRUE, sizeof (STabSelection) );
	int nAdded = 0;
	for (i = 0; i < g_list_length (connection_tab_list); i++) {
		item = g_list_nth (connection_tab_list, i);
		p_ct = (struct ConnectionTab *) item->data;
		char *label = p_ct->connection.name;
		if (!tabIsConnected (p_ct) ) {
			log_write ("Cluster: tab %s is disconnected\n", label);
			continue;
		}
		gtk_list_store_append (list_store_cluster, &iter);
		gtk_list_store_set (list_store_cluster, &iter,
		                    COLUMN_CLUSTER_TERM_SELECTED, FALSE,
		                    COLUMN_CLUSTER_TERM_NAME, label, -1);
		// Init to zero (not selected)
		log_debug ("Adding %d %s...\n", nAdded, label);
		STabSelection tab = { p_ct, 0 };
		g_array_insert_val (tabSelectionArray, nAdded, tab);
		log_write ("Cluster: added %d %s\n", nAdded, label);
		nAdded ++;
	}
	log_write ("Cluster: %d tab/s available\n", nAdded);
	gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), GTK_TREE_MODEL (list_store_cluster) );
	//g_signal_connect (tree_view, "cursor-changed", G_CALLBACK (profile_selected_cb), NULL);
	gtk_widget_show (tree_view);
	//select = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
	//gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
	///*GtkWidget **/GtkTreeModel *tree_model_cluster = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
	/* scrolled window */
	GtkWidget *scrolled_window = GTK_WIDGET (gtk_builder_get_object (builder, "scrolled_terminals") );
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_ETCHED_IN);
	gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);
	// Buttons
	GtkWidget *button_select_all = GTK_WIDGET (gtk_builder_get_object (builder, "button_select_all") );
	g_signal_connect (G_OBJECT (button_select_all), "clicked", G_CALLBACK (cluster_select_all_cb), NULL);
	GtkWidget *button_deselect_all = GTK_WIDGET (gtk_builder_get_object (builder, "button_deselect_all") );
	g_signal_connect (G_OBJECT (button_deselect_all), "clicked", G_CALLBACK (cluster_deselect_all_cb), NULL);
	GtkWidget *button_invert_selection = GTK_WIDGET (gtk_builder_get_object (builder, "button_invert_selection") );
	g_signal_connect (G_OBJECT (button_invert_selection), "clicked", G_CALLBACK (cluster_invert_selection_cb), NULL);
	// Command
	GtkWidget *entry_command = GTK_WIDGET (gtk_builder_get_object (builder, "entry_command") );
	gint w_width, w_height;
	gtk_window_get_size (GTK_WINDOW (dialog), &w_width, &w_height);
	get_monitor_size(GTK_WINDOW (main_window), NULL, &w_height);
	gtk_widget_set_size_request (GTK_WIDGET (dialog), w_width, w_height / 2);
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	//gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_transient_for (GTK_WINDOW (GTK_DIALOG (dialog) ), GTK_WINDOW (main_window) );
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog) ) ), vbox);
	gtk_widget_show_all (gtk_dialog_get_content_area (GTK_DIALOG (dialog) ) );
	gint result = gtk_dialog_run (GTK_DIALOG (dialog) );
	if (result == GTK_RESPONSE_OK) {
		log_write ("Cluster command: %s\n", gtk_entry_get_text (GTK_ENTRY (entry_command) ) );
		for (i = 0; i < tabSelectionArray->len; i++) {
			STabSelection *tab = &g_array_index (tabSelectionArray, STabSelection, i);
			if (tab->selected) {
				log_write ("Sending cluster command to %s\n", tab->pTab->connection.name);
				terminal_write_child_ex (tab->pTab, gtk_entry_get_text (GTK_ENTRY (entry_command) ) );
				terminal_write_child_ex (tab->pTab, "\n");
			}
		}
	} else {
		//retcode = 1; /* cancel */
		//break;
	}
	g_array_free (tabSelectionArray, FALSE);
	gtk_widget_destroy (dialog);
	g_object_unref (G_OBJECT (builder) );
}

void
clipboard_cb (GtkClipboard *clipboard, const gchar *text, gpointer data)
{
	if (text == NULL)
		g_vte_selected_text = NULL;
	else {
		if (g_vte_selected_text)
			free (g_vte_selected_text);
		g_vte_selected_text = (char *) malloc (strlen (text) + 1);
		memcpy (g_vte_selected_text, text, strlen (text) + 1);
		log_debug ("g_vte_selected_text = %s\n", g_vte_selected_text);
	}
}
char *
get_terminal_selection (VteTerminal *terminal)
{
	GdkDisplay *display;
	GtkClipboard *clipboard;
	if (vte_terminal_get_has_selection (terminal) ) {
		vte_terminal_copy_primary (terminal);
		display = gtk_widget_get_display (p_current_connection_tab->vte);
		clipboard = gtk_clipboard_get_for_display (display, GDK_SELECTION_PRIMARY);
		gtk_clipboard_request_text (clipboard, clipboard_cb, NULL);
	} else
		g_vte_selected_text = NULL;
	return (g_vte_selected_text);
}

void
Info ()
{
	int major, minor, micro;
	char sys[256], image_filename[1024];
	char s_linked[1024];
	GtkWidget *dialog;
	GtkBuilder *builder;
	GError *error = NULL;
	char ui[1024];
	builder = gtk_builder_new ();
	sprintf (ui, "%s/credits.glade", globals.data_dir);
	if (gtk_builder_add_from_file (builder, ui, &error) == 0) {
		msgbox_error ("Can't load user interface file:\n%s", error->message);
		g_object_unref (G_OBJECT (builder) );
		return;
	}
	dialog = gtk_dialog_new_with_buttons (("About"), GTK_WINDOW (main_window), GTK_DIALOG_MODAL, "_Ok", GTK_RESPONSE_CLOSE, NULL);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_transient_for (GTK_WINDOW (GTK_DIALOG (dialog) ), GTK_WINDOW (main_window) );
	GtkWidget *vbox_main = GTK_WIDGET (gtk_builder_get_object (builder, "vbox_main") );
	/* logo */
	sprintf (image_filename, "%s/main_icon.png", globals.img_dir);
	GtkWidget *image_logo = GTK_WIDGET (gtk_builder_get_object (builder, "image_logo") );
	gtk_image_set_from_file (GTK_IMAGE (image_logo), image_filename);
	/* package, platform and copyright */
	GtkWidget *label_package = GTK_WIDGET (gtk_builder_get_object (builder, "label_package") );
	GtkWidget *label_platform = GTK_WIDGET (gtk_builder_get_object (builder, "label_platform") );
	gtk_label_set_text (GTK_LABEL (label_package), (PACKAGE VERSION));
	get_system (sys);
	gtk_label_set_text (GTK_LABEL (label_platform), g_strconcat ("For ", sys, NULL) );
	/* credits */
	GtkWidget *label_credits = GTK_WIDGET (gtk_builder_get_object (builder, "label_credits") );
	const gchar *credits = gtk_label_get_text (GTK_LABEL (label_credits) );
	gtk_label_set_markup (GTK_LABEL (label_credits), credits);
	/* libraries */
	GtkWidget *label_libs = GTK_WIDGET (gtk_builder_get_object (builder, "label_libs") );
	major = gtk_get_major_version ();
	minor = gtk_get_minor_version ();
	micro = gtk_get_micro_version ();
	sprintf (s_linked,
	         "GTK+ version %d.%d.%d\n"
	         "VTE version %d.%d.%d\n"
	         "%s\n"
	         "libssh version %s",
	         major, minor, micro,
	         VTE_MAJOR_VERSION, VTE_MINOR_VERSION, VTE_MICRO_VERSION,
	         OPENSSL_VERSION_TEXT,
	         ssh_version (0) );
	gtk_label_set_markup (GTK_LABEL (label_libs), s_linked);
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog) ) ), vbox_main);
	gtk_widget_show_all (gtk_dialog_get_content_area (GTK_DIALOG (dialog) ) );
	gtk_dialog_run (GTK_DIALOG (dialog) );
	gtk_widget_destroy (dialog);
	g_object_unref (G_OBJECT (builder) );
}

void
select_encoding_cb (GtkWidget *wgt, gpointer cbdata)
{
	int i, n_enc;
	if (!p_current_connection_tab)
		return;
	n_enc = sizeof (enc_array) / sizeof (struct EncodingEntry);
	for (i = 0; i < n_enc; i++) {
		if (!strcmp (enc_array[i].name, gtk_menu_item_get_label (GTK_MENU_ITEM (wgt) ) ) ) {
			//vte_terminal_set_encoding (VTE_TERMINAL (p_current_connection_tab->vte), enc_array[i].id);
			terminal_set_encoding (p_current_connection_tab, enc_array[i].id);
			break;
		}
	}
}

void
create_stock_objects ()
{
	GtkIconTheme *ict = gtk_icon_theme_get_default();
	gtk_icon_theme_append_search_path(ict, globals.img_dir);
}

void
get_main_menu ()
{
	GtkBuilder *builder;
	GMenuModel *menu;

	builder = gtk_builder_new();
	gtk_builder_add_from_string(builder, ui_main_desc, -1, NULL);

	action_group = g_simple_action_group_new();
	g_action_map_add_action_entries(G_ACTION_MAP(action_group), main_menu_items, G_N_ELEMENTS(main_menu_items), NULL);

	menu = G_MENU_MODEL(gtk_builder_get_object(builder, "MainMenu"));
	menubar = gtk_menu_bar_new_from_model(menu);

	gtk_widget_insert_action_group(main_window, "lt", G_ACTION_GROUP(action_group));
	g_object_unref (G_OBJECT (builder) );
}
void
add_toolbar (GtkWidget *box)
{
	GtkBuilder *builder;
	gchar rscfile[1024];

	sprintf (rscfile, "%s/toolbar.glade", globals.data_dir);
	builder = gtk_builder_new_from_file(rscfile);
	main_toolbar = GTK_WIDGET(gtk_builder_get_object (builder, "MainToolbar"));
	gtk_toolbar_set_style (GTK_TOOLBAR (main_toolbar), GTK_TOOLBAR_ICONS);
	gtk_box_pack_start (GTK_BOX (box), main_toolbar, FALSE, TRUE, 0);
	gtk_widget_show (main_toolbar);
	g_object_unref (G_OBJECT (builder) );
}

/**
 * update_title() - Updates the window title
 */
void
update_title ()
{
	char title[256];
	char appname[256];
	char label[256];
	if (p_current_connection_tab) {
		strcpy (label, p_current_connection_tab->connection.name);
	} else {
		strcpy (label, "");
	}
	sprintf (appname, "%s %s", PACKAGE, VERSION);
	if (label[0] != 0)
		sprintf (title, "%s - %s", label, appname);
	else
		strcpy (title, appname);
	gtk_window_set_title (GTK_WINDOW (main_window), title);
}

/**
 * update_screen_info() - Refresh title
 */
void
update_screen_info ()
{
	update_title ();
}

void
terminal_popup_menu (GdkEventButton *event)
{
	GtkBuilder *builder;
	GtkWidget *popup;
	GtkWidget *pop1;

	builder = gtk_builder_new();
	gtk_builder_add_from_string(builder, ui_popup_desc, -1, NULL);
	popup = GTK_WIDGET(gtk_builder_get_object (builder, "popup_menu"));
	gtk_menu_popup_at_pointer (GTK_MENU (popup), (GdkEvent*)event);
	//test
	pop1 = GTK_WIDGET(gtk_builder_get_object(builder, "pop1"));
	g_signal_connect(pop1, "activate", test, NULL);
}

gint
delete_event_cb (GtkWidget *window, GdkEventAny *e, gpointer data)
{
	application_quit ();
	return TRUE;
}

/**
 * child_exited_cb() - This signal is emitted when the terminal detects that a child started using vte_terminal_fork_command() has exited
 */
void
child_exited_cb (VteTerminal *vteterminal,
                 gint         status,
                 gpointer     user_data)
{
	struct ConnectionTab *p_ct;
	p_ct = (struct ConnectionTab *) user_data;
	log_write ("%s\n", p_ct->connection.name);
	tabInitConnection (p_ct);
	/* in case of remote connection save it and keep tab, else remove tab */
	connection_copy (&p_ct->last_connection, &p_ct->connection);
	refreshTabStatus (p_ct);
	log_debug ("connection '%s' disconnecting\n", p_ct->connection.name);
	lt_ssh_disconnect (&p_ct->ssh_info);
	terminal_write_ex (p_ct, "\n\rDisconnected. Hit enter to reconnect.\n\r", -1);
	update_screen_info ();
}

/**
 * eof_cb() - Emitted when the terminal receives an end-of-file from a child which is running in the terminal (usually after "child-exited")
 */
void
eof_cb (VteTerminal *vteterminal, gpointer user_data)
{
	struct ConnectionTab *p_ct;
	p_ct = (struct ConnectionTab *) user_data;
	log_write ("[%s] : %s\n", __func__, p_ct->connection.name);
	connection_copy (&p_ct->last_connection, &p_ct->connection);
	tabInitConnection (p_ct);
	update_screen_info ();
	refreshTabStatus (p_ct);
	lt_ssh_disconnect (&p_ct->ssh_info);
}

/**
 * status_line_changed_cb()
 */
void
status_line_changed_cb (VteTerminal *vteterminal, gpointer user_data)
{
	//printf ("status_line_changed_cb() : %s\n", vte_terminal_get_status_line (vteterminal));
}

gboolean
button_press_event_cb (GtkWidget *widget, GdkEventButton *event, gpointer userdata)
{
	if (event->type == GDK_BUTTON_PRESS) {
		if (event->button == 3) {
			log_write ("Right button pressed\n");
			if (prefs.mouse_paste_on_right_button)
				vte_terminal_paste_clipboard (VTE_TERMINAL (userdata /*p_current_connection_tab->vte*/) );
			else
				terminal_popup_menu (event);
			return TRUE;
		}
	}
	return FALSE;
}

void
window_title_changed_cb (VteTerminal *vteterminal, gpointer user_data)
{
	update_title ();
}

void
selection_changed_cb (VteTerminal *vteterminal, gpointer user_data)
{
	if (prefs.mouse_copy_on_select)
		terminal_copy_to_clipboard(VTE_TERMINAL (p_current_connection_tab->vte));
}

void
adjust_font_size (GtkWidget *widget, gint delta)
{
	VteTerminal *terminal;
	PangoFontDescription *desired;
	gint newsize;
	gint columns, rows, owidth, oheight;
	/* Read the screen dimensions in cells. */
	terminal = VTE_TERMINAL (widget);
	columns = vte_terminal_get_column_count (terminal);
	rows = vte_terminal_get_row_count (terminal);
	/* Take into account padding and border overhead. */
	gtk_window_get_size (GTK_WINDOW (main_window), &owidth, &oheight);
	owidth -= vte_terminal_get_char_width (terminal) * columns;
	oheight -= vte_terminal_get_char_height (terminal) * rows;
	/* Calculate the new font size. */
	desired = pango_font_description_copy (vte_terminal_get_font (terminal) );
	newsize = pango_font_description_get_size (desired) / PANGO_SCALE;
	if (delta)
		newsize += delta;
	else
		newsize = globals.original_font_size;
	pango_font_description_set_size (desired, CLAMP (newsize, 4, 144) * PANGO_SCALE);
	/* Change the font, then resize the window so that we have the same
	 * number of rows and columns. */
	vte_terminal_set_font (terminal, desired);
	pango_font_description_free (desired);
}

void
increase_font_size_cb (GtkWidget *widget, gpointer user_data)
{
	adjust_font_size (widget, 1);
}

void
decrease_font_size_cb (GtkWidget *widget, gpointer user_data)
{
	adjust_font_size (widget, -1);
}

gboolean
key_press_event_cb (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	int keyReturn, keyEnter;
	keyReturn = GDK_KEY_Return;
	keyEnter = GDK_KEY_KP_Enter;
	if (event->keyval == keyReturn || event->keyval == keyEnter) {
		if (!p_current_connection_tab)
			return FALSE;
		if (tabGetConnectionStatus (p_current_connection_tab) == TAB_CONN_STATUS_DISCONNECTED &&
		                p_current_connection_tab->last_connection.name[0] != 0) {
			log_debug ("Enter/Return key pressed\n");
			tabInitConnection (p_current_connection_tab);
			p_current_connection_tab->enter_key_relogging = 1;
			log_on (p_current_connection_tab);
			refreshTabStatus (p_current_connection_tab);
			update_screen_info ();
			/* TRUE to stop other handlers from being invoked for the event */
			return TRUE;
		}
	}
	return FALSE;
}

void
update_by_tab (struct ConnectionTab *pTab)
{
	update_screen_info ();
	refreshTabStatus (pTab);
}

void
notebook_switch_page_cb (GtkNotebook *notebook, GtkWidget *page, gint page_num, gpointer user_data)
{
	GtkWidget *child;
	if (!switch_tab_enabled)
		return;
	switch_tab_enabled = FALSE;
	log_write ("Switched to page id: %d\n", page_num);
	//child = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), page_num);
	child = gtk_notebook_get_nth_page (notebook, page_num);
	p_current_connection_tab = get_connection_tab_from_child (child); /* try with g_list_find () */
	log_write ("Page name: %s\n", p_current_connection_tab->connection.name);
	update_by_tab (p_current_connection_tab);
	switch_tab_enabled = TRUE;
}

void
terminal_focus_cb (GtkWidget *widget,
                   gpointer user_data)
{
	struct ConnectionTab *newTab = (struct ConnectionTab *) user_data;
	if (p_current_connection_tab == newTab)
		return;
	log_debug ("%s\n", vte_terminal_get_window_title (VTE_TERMINAL (newTab->vte) ) );
	p_current_connection_tab = newTab;
	update_by_tab (p_current_connection_tab);
}

void
notebook_page_reordered_cb (GtkNotebook *notebook, GtkWidget *child, guint page_num, gpointer user_data)
{
	log_write ("Page reordered: %d\n", page_num);
}
void
apply_preferences ()
{
	int i;
	GList *item;
	GtkWidget *vte;
	struct ConnectionTab *p_ct = NULL;
	for (i = 0; i < g_list_length (connection_tab_list); i++) {
		item = g_list_nth (connection_tab_list, i);
		p_ct = (struct ConnectionTab *) item->data;
		vte = p_ct->vte;
		vte_terminal_set_mouse_autohide (VTE_TERMINAL (vte), prefs.mouse_autohide);
		vte_terminal_set_scrollback_lines (VTE_TERMINAL (vte), prefs.scrollback_lines);
		vte_terminal_set_scroll_on_keystroke (VTE_TERMINAL (vte), prefs.scroll_on_keystroke);
		vte_terminal_set_scroll_on_output (VTE_TERMINAL (vte), prefs.scroll_on_output);
		if (prefs.character_encoding[0] != 0)
			terminal_set_encoding (p_ct, prefs.character_encoding);
		else
			strcpy (prefs.character_encoding, vte_terminal_get_encoding (VTE_TERMINAL (vte) ) );
		//use only default words. I dont know where the exceptions come from.
		vte_terminal_set_word_char_exceptions (VTE_TERMINAL (vte), "");
		gtk_window_set_resizable (GTK_WINDOW (main_window), TRUE);
	}
	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), prefs.tabs_position);
}

void
apply_profile_terminal (GtkWidget *terminal, struct Profile *p_profile)
{
	GdkRGBA fg, bg;
	gdk_rgba_parse (&fg, p_profile->fg_color);
	gdk_rgba_parse (&bg, p_profile->bg_color);
	vte_terminal_set_color_foreground (VTE_TERMINAL (terminal), &fg);
	vte_terminal_set_color_background (VTE_TERMINAL (terminal), &bg);
	if (p_profile->font_use_system) {
		terminal_set_font_from_string (VTE_TERMINAL (terminal), globals.system_font);
	} else if (p_profile->font[0] != 0) {
		terminal_set_font_from_string (VTE_TERMINAL (terminal), p_profile->font);
	}
	if (p_profile->cursor_shape >= 0 && p_profile->cursor_shape <= 2)
		vte_terminal_set_cursor_shape (VTE_TERMINAL (terminal), p_profile->cursor_shape);
	else
		p_profile->cursor_shape = vte_terminal_get_cursor_shape (VTE_TERMINAL (terminal) );
	vte_terminal_set_cursor_blink_mode (VTE_TERMINAL (terminal), p_profile->cursor_blinking ? VTE_CURSOR_BLINK_ON : VTE_CURSOR_BLINK_OFF);
}

void
apply_profile (struct ConnectionTab *p_ct)
{
	apply_profile_terminal (p_ct->vte, &g_profile);
}

void
update_all_profiles ()
{
	int i;
	GList *item;
	struct ConnectionTab *p_ct = NULL;
	for (i = 0; i < g_list_length (connection_tab_list); i++) {
		item = g_list_nth (connection_tab_list, i);
		p_ct = (struct ConnectionTab *) item->data;
		apply_profile (p_ct);
	}
}

/*
  Function: start_gtk
  Creates the main user interface
*/
void
start_gtk (int argc, char **argv)
{
	GtkWidget *vbox;          /* main vbox */
	signal (SIGCHLD, child_exit); /* a child process ends */
	signal (SIGSEGV, segv_handler); /* Segmentation fault */
	connection_tab_list = NULL;
	p_current_connection_tab = NULL;
	gtk_init (&argc, &argv);
	log_write ("Creating main window...\n");
	main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_icon_from_file (GTK_WINDOW (main_window), g_strconcat (globals.img_dir, "/main_icon.png", NULL), NULL);
	g_screen = gtk_widget_get_screen (GTK_WIDGET (main_window) );
	set_title (0);
	if (prefs.maximize)
		gtk_window_maximize (GTK_WINDOW (main_window) );
	/* Create new stock objects */
	log_write ("Creating stock objects...\n");
	create_stock_objects ();
	/* Main vbox */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (main_window), vbox);
	gtk_widget_show_all (vbox);
	/* Menu */
	log_write ("Creating menu...\n");
	get_main_menu ();
	gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, TRUE, 0);
	gtk_widget_show (menubar);
	/* Toolbar */
	log_write ("Creating toolbar...\n");
	add_toolbar(vbox);
	/* Paned window */
	hpaned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	/* list store for connetions */
	connection_init_stuff ();
	/* Notebook */
	notebook = gtk_notebook_new ();
	gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_TOP);
	gtk_notebook_popup_enable (GTK_NOTEBOOK (notebook) );
	gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), TRUE);
	g_signal_connect (notebook, "switch-page", G_CALLBACK (notebook_switch_page_cb), 0);
	g_signal_connect (notebook, "page-reordered", G_CALLBACK (notebook_page_reordered_cb), 0);
	gtk_widget_show (notebook);
	gtk_paned_add2 (GTK_PANED (hpaned), notebook);
	gtk_box_pack_start (GTK_BOX (vbox), hpaned, TRUE, TRUE, 0);
	gtk_widget_show (hpaned);
	g_signal_connect (main_window, "delete_event", G_CALLBACK (delete_event_cb), NULL);
	g_signal_connect (main_window, "key-press-event", G_CALLBACK (key_press_event_cb), NULL);
	gtk_window_set_default_size (GTK_WINDOW (main_window), prefs.w, prefs.h); /* keep this before gtk_widget_show() */
	gtk_widget_show (main_window);
	/* init list of connections for the first time, needed by terminal popup menu */
	log_write ("Loading connections...\n");
	load_connections ();
	/* Ensure that buttons images will be shown */
	GtkSettings *default_settings = gtk_settings_get_default ();
	g_object_set (default_settings, "gtk-button-images", TRUE, NULL);
}
