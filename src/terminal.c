
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
extern struct ConnectionTab *p_current_connection_tab;
extern GList *connection_tab_list;

char *auth_state_desc[] = { "AUTH_STATE_NOT_LOGGED", "AUTH_STATE_GOT_USER", "AUTH_STATE_GOT_PASSWORD", "AUTH_STATE_LOGGED" };

void spawn_cb(VteTerminal *terminal, GPid pid, GError *error, gpointer user_data)
{
	char error_msg[1024];
	struct ConnectionTab *p_conn_tab = (struct ConnectionTab *)user_data;
	if (pid == -1) {
		strcpy(error_msg, error->message);
		tabSetConnectionStatus(p_conn_tab, TAB_CONN_STATUS_DISCONNECTED);
		terminal_write_ex(p_conn_tab, "%s\r\n", error_msg);
		return;
	}
	p_conn_tab->pid = pid;
	tabSetConnectionStatus(p_conn_tab, TAB_CONN_STATUS_CONNECTED);
}
/**
 * log_on() - starts a connection with the given protocol (called by connection_log_on())
 * @return 0 if ok, not zero otherwise
 */
int log_on(struct ConnectionTab *p_conn_tab)
{
	char expanded_args[1024], temp[64];
	char **p_params;
	int ret;
	int prefix_len = 0;
	struct Protocol *p_prot = &globals.ssh_proto;

	p_conn_tab->auth_attempt = 0;
	p_conn_tab->auth_state = AUTH_STATE_NOT_LOGGED;
	log_write("[%s] server:%s\n", __func__, p_conn_tab->connection.host);
	tabSetConnectionStatus(p_conn_tab, TAB_CONN_STATUS_CONNECTING);
	log_write("Init ssh\n");
	p_conn_tab->enter_key_relogging = 0;

#ifdef HAVE_SSHPASS
	if (p_conn_tab->connection.password[0]) {
		char cmd[300];
		prefix_len = sprintf(cmd, "sshpass -p %s %s", p_conn_tab->connection.password, p_prot->command);
		ret = expand_args(&p_conn_tab->connection, p_prot->args, cmd, expanded_args);
	} else
#endif
		ret = expand_args(&p_conn_tab->connection, p_prot->args, p_prot->command, expanded_args);
	if (ret)
		return 1;
	// Add SSH options
	if (p_conn_tab->connection.sshOptions.x11Forwarding)
		strcat(expanded_args, " -X");
	if (p_conn_tab->connection.sshOptions.agentForwarding)
		strcat(expanded_args, " -A");
	if (p_conn_tab->connection.sshOptions.disableStrictKeyChecking)
		strcat(expanded_args, " -o StrictHostKeyChecking=no");
	if (p_conn_tab->connection.sshOptions.flagKeepAlive) {
		sprintf(temp, " -o ServerAliveInterval=%d", p_conn_tab->connection.sshOptions.keepAliveInterval);
		strcat(expanded_args, temp);
	}
	if (p_conn_tab->connection.sshOptions.flagConnectTimeout) {
		sprintf(temp, " -o ConnectTimeout=%d", p_conn_tab->connection.sshOptions.connectTimeout);
		strcat(expanded_args, temp);
	}
	if (p_conn_tab->connection.auth_mode == CONN_AUTH_MODE_KEY
	    && p_conn_tab->connection.identityFile[0]) {
		strcat(expanded_args, " -i \"");
		strcat(expanded_args, p_conn_tab->connection.identityFile);
		strcat(expanded_args, "\"");
	}
	/* Add user options */
	if (p_conn_tab->connection.user_options[0] != 0) {
		strcat(expanded_args, " ");
		strcat(expanded_args, p_conn_tab->connection.user_options);
	}
	/* omit password */
	log_debug("expand_args : %s\n", expanded_args + prefix_len);
	p_params = splitString(expanded_args, " ", TRUE, "\"", TRUE, NULL);
	/*
	 * now the array is something like
	 * char *params[] = { "ssh", "fabio@localhost", NULL };
	 */
	terminal_write_ex(p_conn_tab, "Logging in...\n\r");
	GSpawnFlags spawn_flags = G_SPAWN_SEARCH_PATH;
	vte_terminal_spawn_async(VTE_TERMINAL(p_conn_tab->vte), VTE_PTY_DEFAULT, NULL, p_params, NULL,
	                         spawn_flags, NULL, NULL, NULL, 10000/* 10s */, NULL, spawn_cb, p_conn_tab);
	free(p_params);
	return 0;
}

void terminal_write_ex(struct ConnectionTab *p_ct, const char *fmt, ...)
{
	va_list ap;
	char text[1024];
	va_start(ap, fmt);
	vsprintf(text, fmt, ap);
	va_end(ap);
	if (p_ct)
		vte_terminal_feed(VTE_TERMINAL(p_ct->vte), text, -1);
}

void terminal_write(const char *fmt, ...)
{
	va_list ap;
	char text[1024];
	va_start(ap, fmt);
	vsprintf(text, fmt, ap);
	va_end(ap);
	if (p_current_connection_tab)
		vte_terminal_feed(VTE_TERMINAL(p_current_connection_tab->vte), text, -1);
}

void terminal_write_child_ex(SConnectionTab *pTab, const char *text)
{
	if (pTab)
		vte_terminal_feed_child(VTE_TERMINAL(pTab->vte), text, -1);
}

void terminal_write_child(const char *text)
{
	if (p_current_connection_tab)
		terminal_write_child_ex(p_current_connection_tab, text);
}

#if VTE_CHECK_VERSION(0, 46, 0) && !defined(NO_VTE_PCRE2)
void terminal_set_search_expr(char *expr)
{
	GError* err = NULL;
	if (p_current_connection_tab == NULL)
		return;
	VteRegex *regex = vte_regex_new_for_search(expr, -1, 0, &err);
	if (err) {
		log_write("failed to compile regex: %s\n", expr);
		log_debug("failed to compile regex: %s, %s\n", expr, err->message);
		return;
	}
	vte_terminal_search_set_regex(VTE_TERMINAL(p_current_connection_tab->vte), regex, 0);
}
#else
/* deprecated by vte, but vte with pcre2 is broken on ubuntu for now */
void terminal_set_search_expr(char *expr)
{
	GError* err = NULL;
	if (p_current_connection_tab == NULL)
		return;
	GRegex *regex = g_regex_new(expr, 0, 0, &err);
	if (err) {
		log_write("failed to compile regex: %s\n", expr);
		log_debug("failed to compile regex: %s, %s\n", expr, err->message);
		return;
	}
	vte_terminal_search_set_gregex(VTE_TERMINAL(p_current_connection_tab->vte), regex, 0);
}
#endif

G_MODULE_EXPORT void terminal_find_next()
{
	if (p_current_connection_tab == NULL)
		return;
	vte_terminal_search_find_next(VTE_TERMINAL(p_current_connection_tab->vte));
}

G_MODULE_EXPORT void terminal_find_previous()
{
	if (p_current_connection_tab == NULL)
		return;
	vte_terminal_search_find_previous(VTE_TERMINAL(p_current_connection_tab->vte));
}

int terminal_set_encoding(SConnectionTab *pTab, const char *codeset)
{
	GError *error = NULL;
	vte_terminal_set_encoding(VTE_TERMINAL(pTab->vte), codeset, &error);
	return 0;
}

void terminal_set_font_from_string(VteTerminal *vte, const char *font)
{
	PangoFontDescription *font_desc = pango_font_description_from_string(font);
	vte_terminal_set_font(vte, font_desc);
}


