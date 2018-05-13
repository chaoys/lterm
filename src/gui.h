
/*
 * GUI.H
 */

#ifndef __GUI_H
#define __GUI_H

#include <gtk/gtk.h>
#include <vte/vte.h>
#include <unistd.h>
#include "connection.h"
#include "ssh.h"
#include "profile.h"

#define QUERY_USER 1
#define QUERY_PASSWORD 2
#define QUERY_RENAME 3
#define QUERY_FILE_NEW 4
#define QUERY_FOLDER_NEW 5

enum { TAB_CONN_STATUS_DISCONNECTED = 0, TAB_CONN_STATUS_CONNECTING, TAB_CONN_STATUS_CONNECTED };

#define AUTH_STATE_NOT_LOGGED 0
#define AUTH_STATE_GOT_USER 1
#define AUTH_STATE_GOT_PASSWORD 2
#define AUTH_STATE_LOGGED 3

// Flags
#define TAB_CHANGED 1
#define TAB_LOGGED 2

typedef struct ConnectionTab {
	Connection connection;
	Connection last_connection;
	struct SSH_Info ssh_info;

	int connectionStatus;
	int enter_key_relogging;
	unsigned int auth_state;
	int changes_count;
	int auth_attempt;
	unsigned int flags; // logged, changed

	GtkWidget *hbox_terminal; /* vte + scrollbar */
	GtkWidget *vte;
	GtkWidget *scrollbar;

	GtkWidget *label; // Text
	GtkWidget *notebook; // Notebook containing the terminal

	pid_t pid;
} SConnectionTab;

/* stock objects */

#define MY_STOCK_MAIN_ICON "main_icon"
#define MY_STOCK_TERMINAL "terminal_32"
#define MY_STOCK_DUPLICATE "myduplicate"
#define MY_STOCK_PLUS "plus"
#define MY_STOCK_LESS "less"
#define MY_STOCK_PENCIL "mypencil"
#define MY_STOCK_COPY "copy"
#define MY_STOCK_FOLDER "myfolder"
#define MY_STOCK_UPLOAD "upload"
#define MY_STOCK_DOWNLOAD "download"
#define MY_STOCK_SIDEBAR "sidebar"
#define MY_STOCK_SPLIT_H "splitH"
#define MY_STOCK_SPLIT_V "splitV"
#define MY_STOCK_REGROUP "regroup"
#define MY_STOCK_CLUSTER "cluster"
#define MY_STOCK_PREFERENCES "preferences"
#define MY_STOCK_TRANSFERS "transfers"
#define MY_STOCK_FOLDER_UP "folder_up"
#define MY_STOCK_FOLDER_NEW "folder_new"
#define MY_STOCK_HOME "home"
#define MY_STOCK_FILE_NEW "file_new"


void msgbox_error(const char *fmt, ...);
void msgbox_info(const char *fmt, ...);
gint msgbox_yes_no(const char *fmt, ...);
int query_value(char *title, char *labeltext, char *default_value, char *buffer, int type);
int expand_args(Connection *p_conn, char *args, char *prefix, char *dest);
int show_login_mask(struct ConnectionTab *p_conn_tab, struct SSH_Auth_Data *p_auth);

void tabInitConnection(SConnectionTab *pConn);
char *tabGetConnectionStatusDesc(int status);
void tabSetConnectionStatus(SConnectionTab *pConn, int status);
int tabGetConnectionStatus(SConnectionTab *pConn);
int tabIsConnected(SConnectionTab *pConn);
void tabSetFlag(SConnectionTab *pConn, unsigned int bitmask);
void tabResetFlag(SConnectionTab *pConn, unsigned int bitmask);
unsigned int tabGetFlag(SConnectionTab *pConn, unsigned int bitmask);

void increase_font_size_cb(GtkWidget *widget, gpointer user_data);
void decrease_font_size_cb(GtkWidget *widget, gpointer user_data);
void adjust_font_size(GtkWidget *widget, /*gpointer data,*/ gint delta);
void status_line_changed_cb(VteTerminal *vteterminal, gpointer user_data);
void eof_cb(VteTerminal *vteterminal, gpointer user_data);

void child_exited_cb(VteTerminal *vteterminal, gint status, gpointer user_data);

void size_allocate_cb(GtkWidget *widget, GtkAllocation *allocation, gpointer user_data);
gint delete_event_cb(GtkWidget *window, GdkEventAny *e, gpointer data);
void terminal_popup_menu(GdkEventButton *event);
gboolean button_press_event_cb(GtkWidget *widget, GdkEventButton *event, gpointer userdata);
void window_title_changed_cb(VteTerminal *vteterminal, gpointer user_data);
void selection_changed_cb(VteTerminal *vteterminal, gpointer user_data);
void terminal_focus_cb(GtkWidget       *widget, gpointer         user_data);

void connection_log_on_param(Connection *p_conn);
void connection_log_on();
void connection_log_off();
void connection_duplicate();
void connection_edit_protocols();
void connection_new_terminal_dir(char *directory);
void connection_new_terminal();
void connection_close_tab();
void application_quit();

void edit_copy();
void edit_paste();
void edit_copy_and_paste();
void edit_find();

void edit_select_all();

void terminal_reset();
void terminal_detach_right();
void terminal_detach_down();
void terminal_attach_to_main(struct ConnectionTab *connectionTab);
void terminal_attach_current_to_main();
void terminal_regroup_all();
void terminal_cluster();

void apply_preferences();
void apply_profile();

void help_home_page();
void Info();

void update_screen_info();

void select_current_profile_menu_item(struct ConnectionTab *p_ct);
struct ConnectionTab *get_connection_tab_from_child(GtkWidget *child);
void refreshTabStatus(SConnectionTab *pTab);
int connection_tab_getcwd(struct ConnectionTab *p_ct, char *directory);

void apply_profile(struct ConnectionTab *p_ct);
void apply_profile_terminal(GtkWidget *terminal, struct Profile *p_profile);
void update_all_profiles();

void start_gtk(GApplication *app);
void connection_tab_close(struct ConnectionTab *p_ct);

static inline void get_monitor_size(GtkWindow *win, int *width, int *height)
{
	GdkScreen *screen = gtk_window_get_screen(GTK_WINDOW(win));
	GdkDisplay *dis = gdk_screen_get_display(screen);
	GdkMonitor *mon = gdk_display_get_monitor_at_window(dis, gdk_screen_get_root_window(screen));
	GdkRectangle geome;
	gdk_monitor_get_geometry(mon, &geome);
	if (width)
		*width = geome.width;
	if (height)
		*height = geome.height;
}

#endif
