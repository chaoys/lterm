
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
 * @file terminal.c
 * @brief Terminal functions
 */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <vte/vte.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include "main.h"
#include "connection.h"
#include "preferences.h"
#include "gui.h"
#include "utils.h"
#include "terminal.h"

extern Globals globals;
extern Prefs prefs;
extern GtkWidget *main_window;
extern struct Protocol g_ssh_prot;
extern struct ConnectionTab *p_current_connection_tab;
extern GList *connection_tab_list;

char *auth_state_desc[] = { "AUTH_STATE_NOT_LOGGED", "AUTH_STATE_GOT_USER", "AUTH_STATE_GOT_PASSWORD", "AUTH_STATE_LOGGED" };

int
terminal_connect_ssh (struct ConnectionTab *p_conn_tab, struct SSH_Auth_Data *p_auth)
{
	int login_rc = 0;
	terminal_write_ex (p_conn_tab, "Connecting to %s...\n\r", p_conn_tab->connection.host);
	login_rc = lt_ssh_connect (&p_conn_tab->ssh_info, &globals.ssh_list, p_auth);
	log_debug ("login_rc = %d\n", login_rc);
	if (login_rc == 0) {
		strcpy (p_conn_tab->connection.user, p_auth->user[0] ? p_auth->user : "");
		strcpy (p_conn_tab->connection.password, p_auth->password[0] ? p_auth->password : "");
	} else if (login_rc == SSH_ERR_CONNECT) {
		msgbox_error ("Can't connect to %s", p_conn_tab->connection.host);
	} else if (login_rc == SSH_ERR_UNKNOWN_AUTH_METHOD) {
		//break;
	} else {
		log_write ("ssh: %d %s\n", login_rc, login_rc == 0 ? "" : p_conn_tab->ssh_info.error_s);
	}
	return (login_rc);
}

void spawn_cb(VteTerminal *terminal, GPid pid, GError *error, gpointer user_data)
{
	char error_msg[1024];
	struct ConnectionTab *p_conn_tab = (struct ConnectionTab *)user_data;
	if (pid == -1) {
		strcpy(error_msg, error->message);
		tabSetConnectionStatus (p_conn_tab, TAB_CONN_STATUS_DISCONNECTED);
		terminal_write_ex (p_conn_tab, "%s\r\n", error_msg);
		return;
	}
	p_conn_tab->pid = pid;
	tabSetConnectionStatus (p_conn_tab, TAB_CONN_STATUS_CONNECTED);
}
/**
 * log_on() - starts a connection with the given protocol (called by connection_log_on())
 * @return 0 if ok, not zero otherwise
 */
int
log_on (struct ConnectionTab *p_conn_tab)
{
	char expanded_args[1024], temp[64];
	char **p_params;
	int ret;
	int rc = 0, login_rc = 0;
	struct Protocol *p_prot = &g_ssh_prot;
	struct SSH_Auth_Data auth;
	p_conn_tab->auth_attempt = 0;
	p_conn_tab->auth_state = AUTH_STATE_NOT_LOGGED;
	/* check if command is installed */
	if (!check_command (p_prot->command) ) {
		msgbox_error ("Command not found: %s", p_prot->command);
		return (1);
	}
	log_write ("[%s] server:%s\n", __func__, p_conn_tab->connection.host);
	tabSetConnectionStatus (p_conn_tab, TAB_CONN_STATUS_CONNECTING);
	log_write ("Init ssh\n");
	lt_ssh_init (&p_conn_tab->ssh_info);
	memset (&auth, 0, sizeof (struct SSH_Auth_Data) );
	strcpy (auth.host, p_conn_tab->connection.host);
	auth.port = p_conn_tab->connection.port;
	log_debug("auth.port %d\n", auth.port);
	auth.mode = p_conn_tab->connection.auth_mode;
	log_debug("auth.mode %d\n", auth.mode);
	if (p_conn_tab->connection.user[0]) {
		strcpy (auth.user, p_conn_tab->connection.user);
		log_debug("auth.user %s\n", auth.user);
	}
	if (p_conn_tab->connection.password[0]) {
		strcpy (auth.password, p_conn_tab->connection.password);
		log_debug("auth.password %s\n", auth.password);
	}
	if (p_conn_tab->connection.identityFile[0]) {
		strcpy (auth.identityFile, p_conn_tab->connection.identityFile);
		log_debug("auth.idfile %s\n", auth.identityFile);
	}
	if (p_conn_tab->connection.auth_mode == CONN_AUTH_MODE_KEY) {
		auth.mode = CONN_AUTH_MODE_KEY;
		log_debug ("Log in with key authentication and user %s\n",
				   p_conn_tab->connection.user[0] == 0 ? "unknown" : p_conn_tab->connection.user);
		if (p_conn_tab->connection.user[0] == 0) {
			log_write ("Prompt for username\n");
			rc = show_login_mask (p_conn_tab, &auth);
			strcpy (p_conn_tab->connection.user, auth.user);
			strcpy (p_conn_tab->connection.password, auth.password);
		} else {
			rc = 0;
		}
		if (rc != 0) {
			tabSetConnectionStatus (p_conn_tab, TAB_CONN_STATUS_DISCONNECTED);
			return (1);
		}
	} else if (p_conn_tab->connection.auth_mode == CONN_AUTH_MODE_SAVE || p_conn_tab->enter_key_relogging
			   || (p_conn_tab->connection.user[0] && p_conn_tab->connection.password[0]) ) {
		if (p_conn_tab->enter_key_relogging) {
			log_debug ("Log in again with the same username and password (Enter key pressed).\n");
		}
		else {
			log_debug ("Log in with saved username and password.\n");
		}
		login_rc = terminal_connect_ssh (p_conn_tab, &auth);
		log_debug ("ssh: %s\n", login_rc == 0 ? "authentication ok" : p_conn_tab->ssh_info.error_s);
	} else {
		while (p_conn_tab->auth_attempt < 3) {
			log_debug ("Prompt username and password\n");
			rc = show_login_mask (p_conn_tab, &auth);
			log_debug ("show_login_mask() returns %d\n", rc);
			if (rc == 0) {
				strcpy (p_conn_tab->connection.user, auth.user);
				strcpy (p_conn_tab->connection.password, auth.password);
				rc = 0;
				break;
			}
			tabSetConnectionStatus (p_conn_tab, TAB_CONN_STATUS_DISCONNECTED);
			return (1);
		}
		if (p_conn_tab->auth_attempt >= 3)
			login_rc = 1;
	}
	p_conn_tab->enter_key_relogging = 0;
	if (login_rc) {
		msgbox_error ("%s", p_conn_tab->ssh_info.error_s);
		return (1);
	}

#ifdef HAVE_SSHPASS
	if (p_conn_tab->connection.password[0]) {
		char cmd[300];
		sprintf(cmd, "sshpass -p %s %s", p_conn_tab->connection.password, p_prot->command);
		ret = expand_args (&p_conn_tab->connection, p_prot->args, cmd, expanded_args);
	} else
#endif
		ret = expand_args (&p_conn_tab->connection, p_prot->args, p_prot->command, expanded_args);

	if (ret)
		return 1;
	// Add SSH options
	if (p_conn_tab->connection.sshOptions.x11Forwarding)
		strcat (expanded_args, " -X");
	if (p_conn_tab->connection.sshOptions.agentForwarding)
		strcat (expanded_args, " -A");
	if (p_conn_tab->connection.sshOptions.disableStrictKeyChecking)
		strcat (expanded_args, " -o StrictHostKeyChecking=no");
	if (p_conn_tab->connection.sshOptions.flagKeepAlive) {
		sprintf (temp, " -o ServerAliveInterval=%d", p_conn_tab->connection.sshOptions.keepAliveInterval);
		strcat (expanded_args, temp);
	}
	if (p_conn_tab->connection.auth_mode == CONN_AUTH_MODE_KEY
		&& p_conn_tab->connection.identityFile[0]) {
		strcat (expanded_args, " -i \"");
		strcat (expanded_args, p_conn_tab->connection.identityFile);
		strcat (expanded_args, "\"");
	}
	/* Add user options */
	if (p_conn_tab->connection.user_options[0] != 0) {
		strcat (expanded_args, " ");
		strcat (expanded_args, p_conn_tab->connection.user_options);
	}
	log_debug ("expand_args : %s\n", expanded_args);
	p_params = splitString (expanded_args, " ", TRUE, "\"", TRUE, NULL);
	/*
	 * now the array is something like
	 * char *params[] = { "ssh", "fabio@localhost", NULL };
	 */
	terminal_write_ex (p_conn_tab, "Logging in...\n\r");
	log_debug ("using vte_terminal_fork_command_full()\n");
	GSpawnFlags spawn_flags = G_SPAWN_SEARCH_PATH;
	vte_terminal_spawn_async (VTE_TERMINAL (p_conn_tab->vte), VTE_PTY_DEFAULT, NULL, p_params, NULL,
			spawn_flags, NULL, NULL, NULL, 10000/* 10s */, NULL, spawn_cb, p_conn_tab);
	free (p_params);
	return 0;
}


char *
get_remote_directory_from_vte_title (GtkWidget *vte)
{
	char *title;
	static char directory[1024];
	strcpy (directory, "");
	title = (char *) vte_terminal_get_window_title (VTE_TERMINAL (vte) );
	if (title) {
		title = (char *) strstr (title, ":");
		if (title)
			sscanf (title, ":%s", directory);
	}
	if (directory[0] != 0)
		trim (directory);
	return (directory[0] ? directory : NULL);
}

char *
get_remote_directory ()
{
	if (p_current_connection_tab == NULL)
		return (NULL);
	return (get_remote_directory_from_vte_title (p_current_connection_tab->vte) );
}

void
terminal_write_ex (struct ConnectionTab *p_ct, const char *fmt, ...)
{
	va_list ap;
	char text[1024];
	va_start (ap, fmt);
	vsprintf (text, fmt, ap);
	va_end (ap);
	if (p_ct)
		vte_terminal_feed (VTE_TERMINAL (p_ct->vte), text, -1);
}

void
terminal_write (const char *fmt, ...)
{
	va_list ap;
	char text[1024];
	va_start (ap, fmt);
	vsprintf (text, fmt, ap);
	va_end (ap);
	if (p_current_connection_tab)
		vte_terminal_feed (VTE_TERMINAL (p_current_connection_tab->vte), text, -1);
}

void
terminal_write_child_ex (SConnectionTab *pTab, const char *text)
{
	if (pTab)
		vte_terminal_feed_child (VTE_TERMINAL (pTab->vte), text, -1);
}

void
terminal_write_child (const char *text)
{
	if (p_current_connection_tab)
		terminal_write_child_ex (p_current_connection_tab, text);
}

void
terminal_set_search_expr (char *expr)
{
	GError* err = NULL;
	if (p_current_connection_tab == NULL)
		return;
	VteRegex *regex = vte_regex_new_for_search(expr, -1, 0, &err);
	if (err) {
		log_write ("failed to compile regex: %s\n", expr);
		printf ("failed to compile regex: %s, %s\n", expr, err->message);
		return;
	}
	vte_terminal_search_set_regex (VTE_TERMINAL (p_current_connection_tab->vte), regex, 0);
}

void
terminal_find_next ()
{
	if (p_current_connection_tab == NULL)
		return;
	vte_terminal_search_find_next (VTE_TERMINAL (p_current_connection_tab->vte) );
}

void
terminal_find_previous ()
{
	if (p_current_connection_tab == NULL)
		return;
	vte_terminal_search_find_previous (VTE_TERMINAL (p_current_connection_tab->vte) );
}

int
terminal_set_encoding (SConnectionTab *pTab, const char *codeset)
{
	GError *error = NULL;
	vte_terminal_set_encoding (VTE_TERMINAL (pTab->vte), codeset, &error);
	return 0;
}

void
terminal_set_font_from_string (VteTerminal *vte, const char *font)
{
	PangoFontDescription *font_desc = pango_font_description_from_string (font);
	vte_terminal_set_font (vte, font_desc);
}


