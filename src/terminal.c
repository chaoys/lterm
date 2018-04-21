
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
#include "protocol.h"
#include "connection.h"
#include "preferences.h"
#include "config.h"
#include "gui.h"
#include "utils.h"
#include "terminal.h"

extern Globals globals;
extern Prefs prefs;
extern GtkWidget *main_window;
extern struct Protocol_List g_prot_list;
extern struct ConnectionTab *p_current_connection_tab;
extern GList *connection_tab_list;

char *auth_state_desc[] = { "AUTH_STATE_NOT_LOGGED", "AUTH_STATE_GOT_USER", "AUTH_STATE_GOT_PASSWORD", "AUTH_STATE_LOGGED" };

gboolean
terminal_new (struct ConnectionTab *p_connection_tab, char *directory)
{
	struct Protocol *p_prot;
	//struct ConnectionTab *p_connection_tab;
	gboolean success;
	char error_msg[1024];
	int rc;
	if (directory)
		rc = chdir (directory);
	else if (prefs.local_start_directory[0] != 0)
		rc = chdir (prefs.local_start_directory);
	else
		rc = chdir (globals.home_dir);
	log_debug ("Creating terminal ...\n");
	log_debug ("Using vte_terminal_fork_command_full()\n");
	GError *error = NULL;
	GSpawnFlags spawn_flags;
	const gchar *shell;
	gchar **av;
	//char *av[] = { "/bin/bash", NULL };
	//char **av = 0; g_shell_parse_argv("/bin/bash", 0, &av, 0);
	shell = g_getenv ("SHELL");
	av = g_new (char *, 2);
	av[0] = g_strdup (shell ? shell : "/bin/sh");
	av[1] = NULL;
	log_debug ("Shell: %s\n", av[0]);
	//spawn_flags = G_SPAWN_CHILD_INHERITS_STDIN|G_SPAWN_SEARCH_PATH|G_SPAWN_FILE_AND_ARGV_ZERO;
	spawn_flags = G_SPAWN_CHILD_INHERITS_STDIN | G_SPAWN_SEARCH_PATH;
#if (VTE_CHECK_VERSION(0,38,3) == 1)
	//GCancellable *cancellable = g_cancellable_new ();
	success = vte_terminal_spawn_sync (VTE_TERMINAL (p_connection_tab->vte), VTE_PTY_DEFAULT, NULL, av, NULL,
	                                   spawn_flags,
	                                   NULL, NULL, &p_connection_tab->pid, NULL, &error);
#else
	success = vte_terminal_fork_command_full (VTE_TERMINAL (p_connection_tab->vte), VTE_PTY_DEFAULT, NULL, av, NULL,
	                spawn_flags,
	                NULL, NULL, &p_connection_tab->pid, &error);
#endif
	if (success == FALSE)
		strcpy (error_msg, error->message);
	return (success);
}

int
terminal_connect_ssh (struct ConnectionTab *p_conn_tab, struct SSH_Auth_Data *p_auth)
{
	int login_rc = 0;
	terminal_write_ex (p_conn_tab, _ ("Connecting to %s...\n\r"), p_conn_tab->connection.host);
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

/**
 * log_on() - starts a connection with the given protocol (called by connection_log_on())
 * @return 0 if ok, not zero otherwise
 */
int
log_on (struct ConnectionTab *p_conn_tab)
{
	char expanded_args[1024], temp[64];
	char /*params[64][512],*/ **p_params;
	int i, ret;
	int rc = 0, login_rc = 0;
	struct Protocol *p_prot;
	struct SSH_Auth_Data auth;
	gboolean success;
	char error_msg[1024];
	if ( (p_prot = get_protocol (&g_prot_list, p_conn_tab->connection.protocol) ) == NULL) {
		msgbox_error ("Protocol not found: %s", p_conn_tab->connection.protocol);
		return (1);
	}
	p_conn_tab->auth_attempt = 0;
	p_conn_tab->auth_state = AUTH_STATE_NOT_LOGGED;
	/* check if command is installed */
	if (!check_command (p_prot->command) ) {
		msgbox_error ("Command not found: %s", p_prot->command);
		return (1);
	}
	log_write ("[%s] server:%s protocol:%s\n", __func__, p_conn_tab->connection.host, p_prot->name);
	tabSetConnectionStatus (p_conn_tab, TAB_CONN_STATUS_CONNECTING);
	if (/*!strcmp (p_conn_tab->connection.protocol, "ssh")*/p_prot->type == PROT_TYPE_SSH) {
		log_write ("Init ssh\n");
		lt_ssh_init (&p_conn_tab->ssh_info);
		memset (&auth, 0, sizeof (struct SSH_Auth_Data) );
		strcpy (auth.host, p_conn_tab->connection.host);
		auth.port = p_conn_tab->connection.port;
		auth.mode = p_conn_tab->connection.auth_mode;
		if (p_conn_tab->connection.user[0])
			strcpy (auth.user, p_conn_tab->connection.user);
		if (p_conn_tab->connection.password[0])
			strcpy (auth.password, p_conn_tab->connection.password);
		if (p_conn_tab->connection.identityFile[0])
			strcpy (auth.identityFile, p_conn_tab->connection.identityFile);
		if (p_conn_tab->connection.auth_mode == CONN_AUTH_MODE_KEY) {
			auth.mode = CONN_AUTH_MODE_KEY;
			log_write ("Log in with key authentication and user %s\n",
			           p_conn_tab->connection.user[0] == 0 ? "unknown" : p_conn_tab->connection.user);
			if (p_conn_tab->connection.user[0] == 0) {
				log_write ("Prompt for username\n");
				rc = show_login_mask (p_conn_tab, &auth);
				strcpy (p_conn_tab->connection.user, auth.user);
				strcpy (p_conn_tab->connection.password, auth.password);
			} else {
				rc = 0;
			}
			if (rc == 0) {
			} else { /* cancel */
				tabSetConnectionStatus (p_conn_tab, TAB_CONN_STATUS_DISCONNECTED);
				return (1);
			}
		} else if (p_conn_tab->connection.auth_mode == CONN_AUTH_MODE_SAVE || p_conn_tab->enter_key_relogging
		           || (p_conn_tab->connection.user[0] && p_conn_tab->connection.password[0]) ) {
			if (p_conn_tab->enter_key_relogging)
				log_write ("Log in again with the same username and password (Enter key pressed).\n");
			else
				log_write ("Log in with saved username and password.\n");
			login_rc = terminal_connect_ssh (p_conn_tab, &auth);
			log_write ("ssh: %s\n", login_rc == 0 ? "authentication ok" : p_conn_tab->ssh_info.error_s);
		} else {
			while (p_conn_tab->auth_attempt < 3) {
				log_write ("Prompt username and password\n");
				rc = show_login_mask (p_conn_tab, &auth);
				log_debug ("show_login_mask() returns %d\n", rc);
				if (rc == 0) {
					strcpy (p_conn_tab->connection.user, auth.user);
					strcpy (p_conn_tab->connection.password, auth.password);
					rc = 0;
					break;
				}
				if (rc == 0) {
					login_rc = terminal_connect_ssh (p_conn_tab, &auth);
					if (login_rc == 0)
						break;
					else if (login_rc == SSH_ERR_CONNECT)
						return (1);
					else if (login_rc == SSH_ERR_UNKNOWN_AUTH_METHOD)
						break;
					else
						p_conn_tab->auth_attempt ++;
				} else { // cancel
					tabSetConnectionStatus (p_conn_tab, TAB_CONN_STATUS_DISCONNECTED);
					return (1);
				}
			}
			if (p_conn_tab->auth_attempt >= 3)
				login_rc = 1;
		}
		p_conn_tab->enter_key_relogging = 0;
		if (login_rc) {
			msgbox_error ("%s", p_conn_tab->ssh_info.error_s);
			return (1);
		}
	}
	ret = expand_args (&p_conn_tab->connection, p_prot->args, p_prot->command, expanded_args);
	if (ret)
		return 1;
	// Add SSH options
	if (p_prot->type == PROT_TYPE_SSH) {
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
	}
	/* Add user options */
	if (p_conn_tab->connection.user_options[0] != 0) {
		strcat (expanded_args, " ");
		strcat (expanded_args, p_conn_tab->connection.user_options);
	}
	log_debug ("expand_args : %s\n", expanded_args);
	/*
	  strcpy (params[0], p_prot->command);

	  for (i=1; i<=list_count (expanded_args, ' '); i ++)
	    {
	      list_get_nth_not_null (expanded_args, i, ' ', params[i]);
	    }

	  p_params = (char **) malloc ((list_count (expanded_args, ' ')+2) * sizeof (char *));

	  for (i=0; i<=list_count (expanded_args, ' '); i ++)
	    {
	      p_params[i] = &params[i][0];
	    }

	  p_params[i] = NULL; // trailing null
	*/
	p_params = splitString (expanded_args, " ", TRUE, "\"", TRUE, NULL);
	/*
	 * now the array is something like
	 * char *params[] = { "ssh", "fabio@localhost", NULL };
	 */
#ifdef DEBUG
	log_debug ("forking command : %s ...\n", expanded_args);
	/*i = 0;
	while (p_params[i] != NULL)
	  {
	    log_debug ("'%s' ", p_params[i]);
	    i ++;
	  }

	printf ("\n");*/
#endif
	//vte_terminal_feed (VTE_TERMINAL (p_conn_tab->vte), _("Logging in...\n\r"), -1);
	terminal_write_ex (p_conn_tab, _ ("Logging in...\n\r") );
	log_debug ("using vte_terminal_fork_command_full()\n");
	GError *error = NULL;
	GSpawnFlags spawn_flags;
	spawn_flags = G_SPAWN_SEARCH_PATH;
#if (VTE_CHECK_VERSION(0,38,3) == 1)
	//GCancellable *cancellable = g_cancellable_new ();
	success = vte_terminal_spawn_sync (VTE_TERMINAL (p_conn_tab->vte), VTE_PTY_DEFAULT, NULL, p_params, NULL,
	                                   spawn_flags,
	                                   NULL, NULL, &p_conn_tab->pid, NULL, &error);
#else
	success = vte_terminal_fork_command_full (VTE_TERMINAL (p_conn_tab->vte), VTE_PTY_DEFAULT, NULL, p_params, NULL,
	                spawn_flags, NULL, NULL, &p_conn_tab->pid, &error);
#endif
	if (success == FALSE)
		strcpy (error_msg, error->message);
	log_debug ("Child process id : %d\n", p_conn_tab->pid);
	free (p_params);
	if (success == TRUE) {
		//p_conn_tab->connected = 1;
		tabSetConnectionStatus (p_conn_tab, TAB_CONN_STATUS_CONNECTED);
		p_conn_tab->type = CONNECTION_REMOTE;
		rc = 0;
	} else {
		tabSetConnectionStatus (p_conn_tab, TAB_CONN_STATUS_DISCONNECTED);
		//msgbox_error ("Unable to connect to %s", p_conn_tab->connection.name);
		msgbox_error ("%s", error_msg);
		rc = 2;
	}
	return (rc);
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
	if (p_current_connection_tab->type != CONNECTION_REMOTE)
		return (NULL);
	return (get_remote_directory_from_vte_title (p_current_connection_tab->vte) );
}


/*
 * Next set of functions manage login steps using a Finite State Automaton:
 *
 *  +============+     "login:"/asked_for_user()            +----------+
 *  | NOT_LOGGED |----------------------------------------->| GOT_USER |
 *  +============+           ^                              +----------+
 *        ^  |               |                                   |
 *        |  +-----------------------------------------------+   |"password:"/asked_for_password()
 *        |        "password:"/asked_for_password()          |   |
 *        |                  |                               V   V
 *        |                  |                          +--------------+
 *        |                  +--------------------------| GOT_PASSWORD |
 *        |                   "login:"/asked_for_user() +--------------+
 *        |                                                    |
 *        |                                                    |
 *        |                                                    V
 *        |                log_off or EOF                  +--------+
 *        +------------------------------------------------| LOGGED |
 *                                                         +--------+
 */

int
check_log_in_parameter (int auth_mode, char *auth_param, char *param, char *default_param,
                        int attempt, unsigned int prot_flags, int required_flags,
                        char *label, int query_type, char *log_on_data)
{
	int feed_child;
	if (auth_mode == CONN_AUTH_MODE_SAVE && auth_param[0] /*&& attempt == 1*/) {
		//log_debug ("write auth_param '%s'\n", auth_param);
		strcpy (log_on_data, auth_param);
		feed_child = 1;
	} else if (param[0] /*&& attempt == 1*/) {
		//log_debug ("to be written '%s'\n", param);
		strcpy (log_on_data, param);
		feed_child = 1;
	} else if (prot_flags & required_flags) {
		log_debug ("query user for param\n");
		feed_child = query_value ("Log on", label, default_param, log_on_data, query_type);
	}
	return (feed_child);
}

int
asked_for_user (struct ConnectionTab *p_ct, char *log_on_data)
{
	int feed_child;
	char label[256];
	struct Protocol *p_prot;
	VteTerminal *vteterminal;
	log_debug ("\n");
	p_prot = get_protocol (&g_prot_list, p_ct->connection.protocol);
	vteterminal = VTE_TERMINAL (p_ct->vte);
	feed_child = 0;
	sprintf (label, _ ("Enter user for <b>%s</b>:"), p_ct->connection.name);
	feed_child = check_log_in_parameter (p_ct->connection.auth_mode, p_ct->connection.auth_user,
	                                     p_ct->connection.user, p_ct->connection.last_user,
	                                     p_ct->auth_attempt, p_prot->flags, PROT_FLAG_ASKUSER,
	                                     label, QUERY_USER, log_on_data);
	return (feed_child);
}

int
asked_for_password (struct ConnectionTab *p_ct, char *log_on_data)
{
	int feed_child;
	char label[256];
	struct Protocol *p_prot;
	VteTerminal *vteterminal;
	log_debug ("\n");
	p_prot = get_protocol (&g_prot_list, p_ct->connection.protocol);
	vteterminal = VTE_TERMINAL (p_ct->vte);
	feed_child = 0;
	sprintf (label, _ ("Enter password for <b>%s@%s</b>:"), p_ct->connection.user, p_ct->connection.name);
	feed_child = check_log_in_parameter (p_ct->connection.auth_mode, p_ct->connection.auth_password,
	                                     p_ct->connection.password, "",
	                                     p_ct->auth_attempt, p_prot->flags, AUTH_STATE_GOT_PASSWORD,
	                                     label, QUERY_PASSWORD, log_on_data);
	return (feed_child);
}

int
check_log_in_state (struct ConnectionTab *p_ct, char *line)
{
	int feed_child;
	//char label[256];
	char log_on_data[128];
	//struct Protocol *p_prot;
	VteTerminal *vteterminal;
	if (/*p_ct->logged*/tabGetFlag (p_ct, TAB_LOGGED) )
		return 1;
	log_debug ("state = %s line = '%s'\n", auth_state_desc[p_ct->auth_state], line);
	//p_prot = get_protocol (&g_prot_list, p_ct->connection.protocol);
	vteterminal = VTE_TERMINAL (p_ct->vte);
	feed_child = 0;
	/* With authentication by key, password not needed */
	if (p_ct->connection.auth_mode == CONN_AUTH_MODE_KEY)
		p_ct->auth_state = AUTH_STATE_GOT_PASSWORD;
	switch (p_ct->auth_state) {
		case AUTH_STATE_NOT_LOGGED:
			if (strstr (line, "login:") || strstr (line, "username:") ) {
				log_debug ("Server asking user\n");
				feed_child = asked_for_user (p_ct, log_on_data);
				if (feed_child == -1)
					return 0;
				if (feed_child > 0) {
					//p_ct->auth_attempt ++;
					strcpy (p_ct->connection.user, log_on_data);
					p_ct->auth_state = AUTH_STATE_GOT_USER;
				}
			}
			if (strstr (line, "password:") ) {
				feed_child = asked_for_password (p_ct, log_on_data);
				if (feed_child == -1)
					return 0;
				if (feed_child > 0) {
					strcpy (p_ct->connection.password, log_on_data);
					p_ct->auth_state = AUTH_STATE_GOT_PASSWORD;
				}
			}
			break;
		case AUTH_STATE_GOT_USER:
			if (strstr (line, "password:") ) {
				feed_child = asked_for_password (p_ct, log_on_data);
				if (feed_child == -1)
					return 0;
				if (feed_child > 0) {
					strcpy (p_ct->connection.password, log_on_data);
					p_ct->auth_state = AUTH_STATE_GOT_PASSWORD;
				}
			}
			break;
		case AUTH_STATE_GOT_PASSWORD:
			if (strstr (line, "login:") || strstr (line, "username:") ) {
				p_ct->auth_state = AUTH_STATE_NOT_LOGGED;
				strcpy (p_ct->connection.user, "");
				strcpy (p_ct->connection.password, "");
				log_debug ("Server asking user\n");
				feed_child = asked_for_user (p_ct, log_on_data);
				if (feed_child == -1)
					return 0;
				if (feed_child > 0) {
					//p_ct->auth_attempt ++;
					strcpy (p_ct->connection.user, log_on_data);
					p_ct->auth_state = AUTH_STATE_GOT_USER;
				}
			} else if (strstr (line, "password:") ) {
				strcpy (p_ct->connection.password, "");
				feed_child = asked_for_password (p_ct, log_on_data);
				if (feed_child == -1)
					return 0;
				if (feed_child > 0) {
					strcpy (p_ct->connection.password, log_on_data);
					p_ct->auth_state = AUTH_STATE_GOT_PASSWORD;
				}
			} else if (p_ct->changes_count > strlen ("login:") ) {
				log_debug ("authentication ok %d > %d\n", p_ct->changes_count, (int) strlen ("login:") );
				//p_ct->logged = 1;
				tabSetFlag (p_ct, TAB_LOGGED);
				feed_child = 0;
				p_ct->auth_state = AUTH_STATE_LOGGED;
			}
			break;
		default:
			break;
	}
	if (feed_child) {
		strcat (log_on_data, "\n");
		vte_terminal_feed_child (vteterminal, log_on_data, -1);
		//if (p_ct->auth_state == AUTH_STATE_GOT_PASSWORD)
		//  {
		//if (p_ct->ssh_info.ssh_node)
		vte_terminal_feed (vteterminal, "\n\rAuthenticating. Please wait...\n\r", -1);
		// }
	}
	p_ct->changes_count ++;
	return 1;
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
	//vte_terminal_feed_child (VTE_TERMINAL(p_current_connection_tab->vte), text, -1);
}

void
terminal_set_search_expr (char *expr)
{
	GError* err = NULL;
	if (p_current_connection_tab == NULL)
		return;
	GRegex* regex = g_regex_new (expr, 0, 0, &err);
	if (err) {
		log_write ("failed to compile regex: %s\n", expr);
		return;
	}
#if (VTE_CHECK_VERSION(0,38,3) == 1)
	vte_terminal_search_set_gregex (VTE_TERMINAL (p_current_connection_tab->vte), regex, 0);
#else
	vte_terminal_search_set_gregex (VTE_TERMINAL (p_current_connection_tab->vte), regex);
#endif
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
#if (VTE_CHECK_VERSION(0,38,3) == 1)
	// Since 0.38.3
	GError *error = NULL;
	vte_terminal_set_encoding (VTE_TERMINAL (pTab->vte), codeset, &error);
#else
	// Until 0.38.2
	vte_terminal_set_encoding (VTE_TERMINAL (pTab->vte), codeset);
#endif
	return 0;
}

void
terminal_set_font_from_string (VteTerminal *vte, const char *font)
{
	PangoFontDescription *font_desc = pango_font_description_from_string (font);
	vte_terminal_set_font (vte, font_desc);
}


