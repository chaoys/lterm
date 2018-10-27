
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
 * @file connection.c
 * @brief Connection management
 */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <sys/utsname.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include "profile.h"
#include "gui.h"
#include "connection.h"
#include "main.h"
#include "utils.h"
#include "xml.h"

extern Globals globals;
extern Prefs prefs;
extern GtkWidget *main_window;

GList *conn_list;

GtkWidget *port_spin_button;
GtkWidget *check_x11, *check_agentForwarding;
GtkWidget *check_disable_key_checking, *check_keepAliveInterval, *spin_keepAliveInterval, *check_connect_timeout, *spin_connect_timeout;

struct _AuthWidgets {
	GtkWidget *user_entry, *password_entry;
	GtkWidget *radio_auth_key;
	GtkWidget *entry_private_key;
	GtkWidget *radio_auth_prompt;
	GtkWidget *radio_auth_save;
	GtkWidget *button_select_private_key, *button_clear_private_key;
} authWidgets;

enum {
	NAME_COLUMN,
	HOST_COLUMN,
	PORT_COLUMN,
	N_COLUMNS
};

int conn_update_last_user(char *cname, char *last_user)
{
	Connection *p_conn;
	log_debug("updating last_user = %s for conn. %s\n", last_user, cname);
	p_conn = cl_get_by_name(conn_list, cname);
	if (p_conn)
		strcpy(p_conn->last_user, last_user);
	return 0;
}

static void write_connection_node(FILE *fp, Connection *p_conn, int indent)
{
	fprintf(fp, "%*s<connection name='%s' host='%s' port='%d' flags='%d'>\n"
	        "%*s  <authentication>\n"
	        "%*s    <mode>%d</mode>\n"
	        "%*s    <auth_user>%s</auth_user>\n"
	        "%*s    <auth_password>%s</auth_password>\n"
	        "%*s    <identityFile>%s</identityFile>\n"
	        "%*s  </authentication>\n"
	        "%*s  <last_user>%s</last_user>\n"
	        "%*s  <user_options>%s</user_options>\n"
	        "%*s  <options>\n"
	        "%*s    <property name='x11Forwarding'>%d</property>\n"
	        "%*s    <property name='agentForwarding'>%d</property>\n"
	        "%*s    <property name='disableStrictKeyChecking'>%d</property>\n"
	        "%*s    <property name='keepAliveInterval' enabled='%d'>%d</property>\n"
	        "%*s    <property name='connectTimeout' enabled='%d'>%d</property>\n"
	        "%*s  </options>\n",
	        indent, " ", p_conn->name, NVL(p_conn->host, ""), p_conn->port, p_conn->flags,
	        indent, " ",
	        indent, " ", p_conn->auth_mode,
	        indent, " ", NVL(p_conn->auth_user, ""),
	        indent, " ", NVL(p_conn->auth_password_encrypted, ""),
	        indent, " ", p_conn->identityFile[0] ? g_markup_escape_text(p_conn->identityFile, strlen(p_conn->identityFile)) : "",
	        indent, " ",
	        indent, " ", NVL(p_conn->last_user, ""),
	        indent, " ", p_conn->user_options,
	        indent, " ",
	        indent, " ", p_conn->sshOptions.x11Forwarding,
	        indent, " ", p_conn->sshOptions.agentForwarding,
	        indent, " ", p_conn->sshOptions.disableStrictKeyChecking,
	        indent, " ", p_conn->sshOptions.flagKeepAlive, p_conn->sshOptions.keepAliveInterval,
	        indent, " ", p_conn->sshOptions.flagConnectTimeout, p_conn->sshOptions.connectTimeout,
	        indent, " "
	       );
	fprintf(fp, "%*s</connection>\n", indent, " ");
}

int save_connections(GList *pList, char *filename)
{
	FILE *fp;
	fp = fopen(filename, "w");
	if (fp == 0)
		return 1;
	fprintf(fp,
	        "<?xml version = '1.0'?>\n"
	        "<!DOCTYPE connectionset>\n"
	        "<connectionset version=\"%d\">\n",
	        CFG_XML_VERSION);
	Connection *p_conn;
	GList *item;
	item = g_list_first(pList);
	while (item) {
		p_conn = (Connection *) item->data;
		write_connection_node(fp, p_conn, 2);
		item = g_list_next(item);
	}
	fprintf(fp, "</connectionset>\n");
	fclose(fp);
	return (0);
}

static void read_connection_node(XMLNode *node, Connection *pConn)
{
	char tmp_s[32];
	char propertyName[128], propertyValue[1024];
	XMLNode *child, *node_auth;
	memset(pConn, 0, sizeof(Connection));
	strcpy(pConn->name, xml_node_get_attribute(node, "name"));
	//log_debug ("%s\n", pConn->name);
	strcpy(pConn->host, xml_node_get_attribute(node, "host"));
	strcpy(tmp_s, xml_node_get_attribute(node, "port"));
	if (tmp_s[0])
		pConn->port = atoi(tmp_s);
	strcpy(tmp_s, NVL(xml_node_get_attribute(node, "flags"), ""));
	if (tmp_s[0])
		pConn->flags = atoi(tmp_s);
	if ((child = xml_node_get_child(node, "last_user")))
		strcpy(pConn->last_user, NVL(xml_node_get_value(child), ""));
	if ((node_auth = xml_node_get_child(node, "authentication"))) {
		if ((child = xml_node_get_child(node_auth, "mode"))) {
			strcpy(tmp_s, NVL(xml_node_get_value(child), "0"));
			if (tmp_s[0])
				pConn->auth_mode = atoi(tmp_s);
		}
		if ((child = xml_node_get_child(node_auth, "auth_user")))
			strcpy(pConn->auth_user, NVL(xml_node_get_value(child), ""));
		if ((child = xml_node_get_child(node_auth, "auth_password"))) {
			strcpy(pConn->auth_password_encrypted, NVL(xml_node_get_value(child), ""));
			strcpy(pConn->auth_password, password_decode(pConn->auth_password_encrypted));
		}
		if ((child = xml_node_get_child(node_auth, "identityFile")))
			strcpy(pConn->identityFile, NVL(xml_node_get_value(child), ""));
	}
	if ((child = xml_node_get_child(node, "options"))) {
		XMLNode *propNode = child->children;
		while (propNode) {
			strcpy(propertyName, xml_node_get_attribute(propNode, "name"));
			strcpy(propertyValue, NVL(xml_node_get_value(propNode), "0"));
			//log_debug ("%s = %s\n", propertyName, propertyValue);
			if (!strcmp(propertyName, "x11Forwarding"))
				pConn->sshOptions.x11Forwarding = atoi(propertyValue);
			else if (!strcmp(propertyName, "agentForwarding"))
				pConn->sshOptions.agentForwarding = atoi(propertyValue);
			else if (!strcmp(propertyName, "disableStrictKeyChecking"))
				pConn->sshOptions.disableStrictKeyChecking = atoi(propertyValue);
			else if (!strcmp(propertyName, "keepAliveInterval")) {
				pConn->sshOptions.flagKeepAlive = atoi(NVL(xml_node_get_attribute(propNode, "enabled"), "0"));
				pConn->sshOptions.keepAliveInterval = atoi(propertyValue);
			} else if (!strcmp(propertyName, "connectTimeout")) {
				pConn->sshOptions.flagConnectTimeout = atoi(NVL(xml_node_get_attribute(propNode, "enabled"), "0"));
				pConn->sshOptions.connectTimeout = atoi(propertyValue);
			}
			propNode = propNode->next;
		}
	}
}

static int get_xml_doc(char *filename, XML *xmldoc)
{
	char line[2048];
	char *xml;
	FILE *fp;
	fp = fopen(filename, "r");
	if (fp == NULL)
		return (1);
	xml = (char *) malloc(2048);
	strcpy(xml, "");
	while (fgets(line, 1024, fp) != 0) {
		if (strlen(xml) + strlen(line) > sizeof(xml))
			xml = (char *) realloc(xml, strlen(xml) + strlen(line) + 1);
		strcat(xml, line);
	}
	fclose(fp);
	/* Parse xml and create the connections tree */
	xml_parse(xml, xmldoc);
	if (xmldoc->error.code) {
		log_write("%s\n", xmldoc->error.message);
		return 1;
	}
	return 0;
}

/* loads connections into a list */
static GList * load_connection_list_from_file_xml(char *filename)
{
	int rc = 0;
	Connection *pConn;
	XMLNode *node;
	GList *list = NULL;
	XML xmldoc;
	rc = get_xml_doc(filename, &xmldoc);
	if (rc != 0)
		return (NULL);
	if (xmldoc.cur_root) {
		if (xmldoc.cur_root && strcmp(xmldoc.cur_root->name, "connectionset")) {
			log_write("[%s] can't find root node: connectionset\n", __func__);
			return NULL;
		}
		node = xmldoc.cur_root->children;
		while (node) {
			//log_debug ("%s\n", node->name);
			if (!strcmp(node->name, "connection")) {
				pConn = (Connection *) malloc(sizeof(Connection));
				read_connection_node(node, pConn);
				list = g_list_append(list, pConn);
				//log_debug ("%s@%s\n", pConn->user, pConn->host);
			}
			node = node->next;
		}
		xml_free(&xmldoc);
	}
	return (list);
}
/* load_connections() - loads user connection tree */
int load_connections()
{
	conn_list = load_connection_list_from_file_xml(globals.connections_xml);
	return conn_list ? 0 : -1;
}

/* ---[ Graphic User Interface section ]--- */

static void set_private_key_controls(gboolean status)
{
	gtk_widget_set_state_flags(authWidgets.entry_private_key,
	                           status ? GTK_STATE_FLAG_NORMAL : GTK_STATE_FLAG_INSENSITIVE,
	                           TRUE);
	gtk_widget_set_state_flags(authWidgets.button_select_private_key,
	                           status ? GTK_STATE_FLAG_NORMAL : GTK_STATE_FLAG_INSENSITIVE,
	                           TRUE);
	gtk_widget_set_state_flags(authWidgets.button_clear_private_key,
	                           status ? GTK_STATE_FLAG_NORMAL : GTK_STATE_FLAG_INSENSITIVE,
	                           TRUE);
}

static void radio_auth_save_cb(GtkToggleButton *togglebutton, gpointer user_data)
{
	gtk_widget_set_state_flags(authWidgets.user_entry,
	                           gtk_toggle_button_get_active(togglebutton) ? GTK_STATE_FLAG_NORMAL : GTK_STATE_FLAG_INSENSITIVE,
	                           TRUE);
	gtk_widget_set_state_flags(authWidgets.password_entry,
	                           gtk_toggle_button_get_active(togglebutton) ? GTK_STATE_FLAG_NORMAL : GTK_STATE_FLAG_INSENSITIVE,
	                           TRUE);
}

static void radio_auth_key_cb(GtkToggleButton *togglebutton, gpointer user_data)
{
	set_private_key_controls(gtk_toggle_button_get_active(togglebutton));
}

static void select_private_key_cb(GtkButton *button, gpointer user_data)
{
	GtkWidget *dialog;
	gint result;
	dialog = gtk_file_chooser_dialog_new("Select private key file", GTK_WINDOW(main_window),
	                                     GTK_FILE_CHOOSER_ACTION_OPEN,
	                                     "_Cancel", GTK_RESPONSE_CANCEL,
	                                     "_Open", GTK_RESPONSE_ACCEPT,
	                                     NULL);
	gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dialog), TRUE);
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), FALSE);
	result = gtk_dialog_run(GTK_DIALOG(dialog));
	if (result == GTK_RESPONSE_ACCEPT) {
		gtk_entry_set_text(GTK_ENTRY(authWidgets.entry_private_key), gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)));
	}
	gtk_widget_destroy(dialog);
}

static void clear_private_key_cb(GtkButton *button, gpointer user_data)
{
	gtk_entry_set_text(GTK_ENTRY(authWidgets.entry_private_key), "");
}

#define ERR_VALIDATE_MISSING_VALUES 1
#define ERR_VALIDATE_EXISTING_CONNECTION 2
static char * get_validation_error_string(int error_code)
{
	switch (error_code) {
	case 0:
		return (("Validation OK"));
		break;
	case ERR_VALIDATE_MISSING_VALUES:
		return (("Missing values"));
		break;
	case ERR_VALIDATE_EXISTING_CONNECTION:
		return (("An existing connection has the same name"));
		break;
	default:
		return (("Name is not allowed"));
		break;
	}
}

static int validate_name(Connection *p_conn, char *item_name)
{
	Connection *p_conn_ctrl;
	if (item_name[0] == 0)
		return (ERR_VALIDATE_MISSING_VALUES);
	if (p_conn) { /* adding or updating a connection */
		if (p_conn->host[0] == 0)
			return (ERR_VALIDATE_MISSING_VALUES);
		p_conn_ctrl = cl_get_by_name(conn_list, item_name);
		if (p_conn_ctrl && p_conn_ctrl != p_conn) {
			return (ERR_VALIDATE_EXISTING_CONNECTION);
		}
	}
	return 0;
}

/* add_update_connection() - add or update connections */
static int add_update_connection(Connection *p_conn)
{
	GtkBuilder *builder;
	GError *error = NULL;
	GtkWidget *notebook;
	char title[64];
	char connection_name[1024];
	int err_name_validation;
	GtkWidget *dialog;
	GtkWidget *name_entry, *host_entry, *user_options_entry;
	gint result;
	Connection conn_new;
	char ui[600];
	int rc;
	log_debug("Loading gui\n");
	builder = gtk_builder_new();
	sprintf(ui, "%s/edit-connection.glade", globals.data_dir);
	if (gtk_builder_add_from_file(builder, ui, &error) == 0) {
		msgbox_error("Can't load user interface file:\n%s", error->message);
		return -1;
	}
	log_debug("Loaded %s\n", ui);
	if (!p_conn) {
		strcpy(title, ("Add connection"));
	} else {
		strcpy(title, ("Edit connection"));
		log_debug("Editing %s\n", p_conn->name);
	}
	log_debug("%s\n", title);
	notebook = GTK_WIDGET(gtk_builder_get_object(builder, "notebook1"));
	/* basic */
	name_entry = GTK_WIDGET(gtk_builder_get_object(builder, "entry_name"));
	if (p_conn) {
		gtk_entry_set_text(GTK_ENTRY(name_entry), p_conn->name);
	}
	host_entry = GTK_WIDGET(gtk_builder_get_object(builder, "entry_host"));
	if (p_conn)
		gtk_entry_set_text(GTK_ENTRY(host_entry), p_conn->host);
	port_spin_button = GTK_WIDGET(gtk_builder_get_object(builder, "spin_port"));
	// X11 Forwarding
	check_x11 = GTK_WIDGET(gtk_builder_get_object(builder, "check_x11forwarding"));
	if (p_conn)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_x11), p_conn->sshOptions.x11Forwarding);
	// Agent forwarding
	check_agentForwarding = GTK_WIDGET(gtk_builder_get_object(builder, "check_agentForwarding"));
	if (p_conn)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_agentForwarding), p_conn->sshOptions.agentForwarding);
	// Disable strict host key checking
	check_disable_key_checking = GTK_WIDGET(gtk_builder_get_object(builder, "check_disableStrictKeyChecking"));
	if (p_conn)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_disable_key_checking), p_conn->sshOptions.disableStrictKeyChecking);
	// Keep alive interval
	check_keepAliveInterval = GTK_WIDGET(gtk_builder_get_object(builder, "check_keepAliveInterval"));
	spin_keepAliveInterval = GTK_WIDGET(gtk_builder_get_object(builder, "spin_keepAliveInterval"));
	check_connect_timeout = GTK_WIDGET(gtk_builder_get_object(builder, "check_connect_timeout"));
	spin_connect_timeout = GTK_WIDGET(gtk_builder_get_object(builder, "spin_connect_timeout"));
	if (p_conn) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_keepAliveInterval), p_conn->sshOptions.flagKeepAlive);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_keepAliveInterval), p_conn->sshOptions.keepAliveInterval);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_connect_timeout), p_conn->sshOptions.flagConnectTimeout);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_connect_timeout), p_conn->sshOptions.connectTimeout);
	}
	// Key authentication (need to be created before sig_handler_prot)
	authWidgets.radio_auth_key = GTK_WIDGET(gtk_builder_get_object(builder, "radio_auth_key"));
	authWidgets.user_entry = GTK_WIDGET(gtk_builder_get_object(builder, "entry_user"));
	authWidgets.password_entry = GTK_WIDGET(gtk_builder_get_object(builder, "entry_password"));
	if (p_conn) {
		gtk_entry_set_text(GTK_ENTRY(authWidgets.user_entry), p_conn->auth_user);
		gtk_entry_set_text(GTK_ENTRY(authWidgets.password_entry), p_conn->auth_password);
	}
	authWidgets.entry_private_key = GTK_WIDGET(gtk_builder_get_object(builder, "entry_private_key"));
	if (p_conn)
		gtk_entry_set_text(GTK_ENTRY(authWidgets.entry_private_key), p_conn->identityFile);
	authWidgets.button_select_private_key = GTK_WIDGET(gtk_builder_get_object(builder, "button_select_private_key"));
	g_signal_connect(G_OBJECT(authWidgets.button_select_private_key), "clicked", G_CALLBACK(select_private_key_cb), NULL);
	authWidgets.button_clear_private_key = GTK_WIDGET(gtk_builder_get_object(builder, "button_clear_private_key"));
	g_signal_connect(G_OBJECT(authWidgets.button_clear_private_key), "clicked", G_CALLBACK(clear_private_key_cb), NULL);
	if (p_conn) {
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(port_spin_button), p_conn->port);
	} else {
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(port_spin_button), globals.ssh_proto.port);
	}
	/* Authentication */
	authWidgets.radio_auth_prompt = GTK_WIDGET(gtk_builder_get_object(builder, "radio_auth_prompt"));
	authWidgets.radio_auth_save = GTK_WIDGET(gtk_builder_get_object(builder, "radio_auth_save"));
	// Private key
	int authMode = p_conn ? p_conn->auth_mode : CONN_AUTH_MODE_PROMPT;
	log_debug("authMode = %d\n", authMode);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(authWidgets.radio_auth_prompt), authMode == CONN_AUTH_MODE_PROMPT);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(authWidgets.radio_auth_save), authMode == CONN_AUTH_MODE_SAVE);
	radio_auth_save_cb(GTK_TOGGLE_BUTTON(authWidgets.radio_auth_save), NULL);   // Force signal
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(authWidgets.radio_auth_key), authMode == CONN_AUTH_MODE_KEY);
	radio_auth_key_cb(GTK_TOGGLE_BUTTON(authWidgets.radio_auth_key), NULL);   // Force signal
	g_signal_connect(authWidgets.radio_auth_save, "toggled", G_CALLBACK(radio_auth_save_cb), NULL);
	g_signal_connect(authWidgets.radio_auth_key, "toggled", G_CALLBACK(radio_auth_key_cb), NULL);
	/* Extra options */
	user_options_entry = GTK_WIDGET(gtk_builder_get_object(builder, "entry_extra_options"));
	if (p_conn) {
		gtk_entry_set_text(GTK_ENTRY(user_options_entry), p_conn->user_options);
	}
	/* create dialog */
	dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog), title);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(GTK_DIALOG(dialog)), GTK_WINDOW(main_window));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_box_pack_end(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), notebook, TRUE, TRUE, 0);
	gtk_dialog_add_buttons(GTK_DIALOG(dialog), "Cancel", GTK_RESPONSE_CANCEL, "Ok", GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
	gtk_widget_show_all(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
	gtk_widget_grab_focus(name_entry);
	/* run dialog */
	while (1) {
		result = gtk_dialog_run(GTK_DIALOG(dialog));
		if (result == GTK_RESPONSE_OK) {
			strcpy(connection_name, gtk_entry_get_text(GTK_ENTRY(name_entry)));
			trim(connection_name);
			/* initialize a new connection structure */
			memset(&conn_new, 0x00, sizeof(Connection));
			/*
			 * if we are updating an existing connection, make a copy before updating
			 * to keep some values
			 */
			if (p_conn)
				connection_copy(&conn_new, p_conn);
			/* update values */
			strcpy(conn_new.name, connection_name);
			strcpy(conn_new.host, gtk_entry_get_text(GTK_ENTRY(host_entry)));
			trim(conn_new.host);
			conn_new.port = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(port_spin_button));
			strcpy(conn_new.user_options, gtk_entry_get_text(GTK_ENTRY(user_options_entry)));
			conn_new.sshOptions.x11Forwarding = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_x11)) ? 1 : 0;
			conn_new.sshOptions.agentForwarding = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_agentForwarding)) ? 1 : 0;
			conn_new.sshOptions.disableStrictKeyChecking = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_disable_key_checking)) ? 1 : 0;
			conn_new.sshOptions.flagKeepAlive = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_keepAliveInterval)) ? 1 : 0;
			conn_new.sshOptions.keepAliveInterval = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_keepAliveInterval));
			conn_new.sshOptions.flagConnectTimeout = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_connect_timeout)) ? 1 : 0;
			conn_new.sshOptions.connectTimeout = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_connect_timeout));
			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(authWidgets.radio_auth_prompt)))
				conn_new.auth_mode = CONN_AUTH_MODE_PROMPT;
			else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(authWidgets.radio_auth_save)))
				conn_new.auth_mode = CONN_AUTH_MODE_SAVE;
			else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(authWidgets.radio_auth_key)))
				conn_new.auth_mode = CONN_AUTH_MODE_KEY;
			strcpy(conn_new.auth_user, gtk_entry_get_text(GTK_ENTRY(authWidgets.user_entry)));
			strcpy(conn_new.auth_password, gtk_entry_get_text(GTK_ENTRY(authWidgets.password_entry)));
			if (conn_new.auth_password[0] != 0)
				strcpy(conn_new.auth_password_encrypted, password_encode(conn_new.auth_password));
			// Private key
			strcpy(conn_new.identityFile, gtk_entry_get_text(GTK_ENTRY(authWidgets.entry_private_key)));
			if (p_conn) { /* edit */
				log_debug("Edit\n");
				log_debug("Validating %s ...\n", connection_name);
				err_name_validation = validate_name(p_conn, connection_name);
				if (!err_name_validation) {
					log_debug("Name validated\n");
					connection_copy(p_conn, &conn_new);
					rc = 0;
					break;
				} else
					msgbox_error(get_validation_error_string(err_name_validation));
			} else {    /* add */
				log_debug("add one connection %s %s %d\n", conn_new.name, conn_new.host, conn_new.port);
				err_name_validation = validate_name(&conn_new, connection_name);
				if (!err_name_validation) {
					cl_insert_sorted(&conn_list, &conn_new);
					save_connections(conn_list, globals.connections_xml);
					rc = 0;
					break;
				} else
					msgbox_error(get_validation_error_string(err_name_validation));
			}
		} else {
			log_debug("user clicked CANCEL\n");
			rc = 1;
			break;
		}
	}
	gtk_widget_destroy(dialog);
	g_object_unref(G_OBJECT(builder));
	return rc;
}

static void create_connections_tree_view(GtkTreeView *tree_view)
{
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GtkListStore *ls;
	ls = gtk_list_store_new(N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
	gtk_tree_view_set_model(tree_view, GTK_TREE_MODEL(ls));
	/* Name */
	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Name", cell, "text", NAME_COLUMN, NULL);
	gtk_tree_view_append_column(tree_view, GTK_TREE_VIEW_COLUMN(column));
	/* Address */
	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Host", cell, "text", HOST_COLUMN, NULL);
	gtk_tree_view_append_column(tree_view, GTK_TREE_VIEW_COLUMN(column));
	/* Port */
	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Port", cell, "text", PORT_COLUMN, NULL);
	gtk_tree_view_append_column(tree_view, GTK_TREE_VIEW_COLUMN(column));
}

static void treeview_add_one_conn(gpointer data, gpointer userdata)
{
	GtkListStore *ls = (GtkListStore *)userdata;
	Connection *c = (Connection *)data;
	GtkTreeIter iter;
	gtk_list_store_append(ls, &iter);
	gtk_list_store_set(ls, &iter, NAME_COLUMN, c->name, HOST_COLUMN, c->host, PORT_COLUMN, c->port, -1);
}
static void update_connections_tree_view(GtkTreeView *tv)
{
	GtkListStore *ls = GTK_LIST_STORE(gtk_tree_view_get_model(tv));
	gtk_list_store_clear(ls);
	g_list_foreach(conn_list, treeview_add_one_conn, ls);
}

static gboolean conn_key_press_cb(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	int keyReturn, keyEnter, keyEsc;
	keyReturn = GDK_KEY_Return;
	keyEnter = GDK_KEY_KP_Enter;
	keyEsc = GDK_KEY_Escape;
	if (event->keyval == keyReturn || event->keyval == keyEnter) {
		gtk_dialog_response(GTK_DIALOG(widget), GTK_RESPONSE_OK);
	}
	if (event->keyval == keyEsc) {
		gtk_dialog_response(GTK_DIALOG(widget), GTK_RESPONSE_CANCEL);
	}
	return FALSE;
}

/* when a row has been double-clicked in the dialog */
static void row_activated_cb(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
	gtk_dialog_response(GTK_DIALOG(user_data), GTK_RESPONSE_OK);
}

static Connection *get_selected_connection(GtkTreeView *tree_view)
{
	GtkTreeIter iter;
	gchar *sel_name;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	Connection *p_conn_selected = NULL;
	model = gtk_tree_view_get_model(tree_view);
	selection = gtk_tree_view_get_selection(tree_view);
	if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		gtk_tree_model_get(model, &iter, NAME_COLUMN, &sel_name, -1);
		p_conn_selected = cl_get_by_name(conn_list, sel_name);
		log_debug("selected %s\n", sel_name);
		g_free(sel_name);
	}
	return p_conn_selected;
}

static void add_button_clicked_cb(GtkButton *button, gpointer user_data)
{
	GtkTreeView *tree_view = user_data;
	if (add_update_connection(NULL) == 0)
		update_connections_tree_view(tree_view);
}

static void edit_button_clicked_cb(GtkButton *button, gpointer user_data)
{
	GtkTreeView *tree_view = user_data;
	Connection *c;
	c = get_selected_connection(tree_view);
	if (c == NULL)
		return;
	if (add_update_connection(c) == 0)
		update_connections_tree_view(tree_view);
}

static void delete_button_clicked_cb(GtkButton *button, gpointer user_data)
{
	int rc;
	Connection *c;
	GtkTreeView *tree_view = user_data;
	char confirm_remove_message[512];
	c = get_selected_connection(tree_view);
	if (c == NULL)
		return;
	sprintf(confirm_remove_message, "Remove connection '%s'?", c->name);
	rc = msgbox_yes_no(confirm_remove_message);
	if (rc == GTK_RESPONSE_YES) {
		log_debug("delete one connection %s %s %d\n", c->name, c->host, c->port);
		cl_remove(&conn_list, c->name);
		update_connections_tree_view(tree_view);
		save_connections(conn_list, globals.connections_xml);
	}
}

int choose_manage_connection(Connection *p_conn)
{
	GtkBuilder *builder;
	GtkWidget *dialog;
	GtkWidget *tv;
	GtkWidget *add_button, *del_button, *edit_button;
	char ui[1024];
	int rc = 1;
	builder = gtk_builder_new();
	sprintf(ui, "%s/connections.glade", globals.data_dir);
	if (gtk_builder_add_from_file(builder, ui, NULL) == 0) {
		msgbox_error("Can't load user interface file:\n%s", ui);
		g_object_unref(builder);
		return -1;
	}
	dialog = GTK_WIDGET(gtk_builder_get_object(builder, "conn_dialog"));
	tv = GTK_WIDGET(gtk_builder_get_object(builder, "conn_tv"));
	add_button = GTK_WIDGET(gtk_builder_get_object(builder, "conn_add"));
	del_button = GTK_WIDGET(gtk_builder_get_object(builder, "conn_del"));
	edit_button = GTK_WIDGET(gtk_builder_get_object(builder, "conn_edit"));
	gtk_dialog_add_buttons(GTK_DIALOG(dialog), "Cancel", GTK_RESPONSE_CANCEL, "Connect", GTK_RESPONSE_OK, NULL);
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(main_window));
	create_connections_tree_view(GTK_TREE_VIEW(tv));
	update_connections_tree_view(GTK_TREE_VIEW(tv));
	g_signal_connect(dialog, "key-press-event", G_CALLBACK(conn_key_press_cb), NULL);
	g_signal_connect(tv, "row-activated", G_CALLBACK(row_activated_cb), dialog);
	g_signal_connect(G_OBJECT(add_button), "clicked", G_CALLBACK(add_button_clicked_cb), tv);
	g_signal_connect(G_OBJECT(del_button), "clicked", G_CALLBACK(delete_button_clicked_cb), tv);
	g_signal_connect(G_OBJECT(edit_button), "clicked", G_CALLBACK(edit_button_clicked_cb), tv);
	while (1) {
		if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
			Connection *c = get_selected_connection(GTK_TREE_VIEW(tv));
			if (c == NULL)
				continue;
			connection_copy(p_conn, c);
			rc = 0;
			log_debug("selected %s %s %d\n", p_conn->name, p_conn->host, p_conn->port);
			break;
		} else {
			rc = 1;
			break;
		}
	}
	gtk_widget_destroy(dialog);
	g_object_unref(builder);
	return rc;
}

