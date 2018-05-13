
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
 * @file ssh.c
 * @brief SSH module
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <libgen.h>
#include <errno.h>
#include "main.h"
#include "utils.h"
#include "ssh.h"
#include "connection.h"

extern Globals globals;
extern Prefs prefs;

/* SSH List functions */

void ssh_list_init(struct SSH_List *p_ssh_list)
{
	p_ssh_list->head = NULL;
	p_ssh_list->tail = NULL;
}

void ssh_list_release_chain(struct SSH_Node *p_head)
{
	if (p_head) {
		ssh_list_release_chain(p_head->next);
		free(p_head);
	}
}

void ssh_list_release(struct SSH_List *p_ssh_list)
{
	if (!p_ssh_list->head)
		return;
	ssh_list_release_chain(p_ssh_list->head);
	p_ssh_list->head = 0;
	p_ssh_list->tail = 0;
}

struct SSH_Node *
ssh_list_append(struct SSH_List *p_ssh_list, struct SSH_Node *p_new)
{
	struct SSH_Node *p_new_decl;
	p_new_decl = (struct SSH_Node *) malloc(sizeof(struct SSH_Node));
	memset(p_new_decl, 0, sizeof(struct SSH_Node));
	memcpy(p_new_decl, p_new, sizeof(struct SSH_Node));
	p_new_decl->next = 0;
	if (p_ssh_list->head == 0) {
		p_ssh_list->head = p_new_decl;
		p_ssh_list->tail = p_new_decl;
	} else {
		p_ssh_list->tail->next = p_new_decl;
		p_ssh_list->tail = p_new_decl;
	}
	return (p_new_decl);
}

void ssh_list_remove(struct SSH_List *p_ssh_list, struct SSH_Node *p_node)
{
	struct SSH_Node *p_del, *p_prec;
	p_prec = NULL;
	p_del = p_ssh_list->head;
	while (p_del) {
		if (p_node == p_del) {
			if (p_prec)
				p_prec->next = p_del->next;
			else
				p_ssh_list->head = p_del->next;
			if (p_ssh_list->tail == p_del)
				p_ssh_list->tail = p_prec;
			free(p_del);
			break;
		}
		p_prec = p_del;
		p_del = p_del->next;
	}
}

struct SSH_Node *
ssh_list_search(struct SSH_List *p_ssh_list, char *host, char *user)
{
	struct SSH_Node *node;
	node = p_ssh_list->head;
	while (node) {
		if (!strcmp(node->host, host) && !strcmp(node->user, user))
			return (node);
		node = node->next;
	}
	return (NULL);
}

void ssh_list_dump(struct SSH_List *p_ssh_list)
{
	struct SSH_Node *node;
	int valid;
	node = p_ssh_list->head;
	while (node) {
		valid = ssh_is_connected(node->session) && node->valid;
		if (valid) {
			ssh_channel c;
			if ((c = ssh_node_open_channel(node))) {
				ssh_channel_close(c);
				ssh_channel_free(c);
			} else
				valid = 0;
		}
		printf("%s@%s (%d) %s\n",
		       node->user, node->host, node->refcount,
		       valid ? "valid" : "invalid");
		node = node->next;
	}
}

/**
 * ssh_node_connect() - connect to server and add node to list
 */
struct SSH_Node *
ssh_node_connect(struct SSH_List *p_ssh_list, struct SSH_Auth_Data *p_auth)
{
	struct SSH_Node node, *p_node = NULL;
	GError *error = NULL;
	int rc, valid = 0;
	long timeout = 3;
	memset(&node, 0, sizeof(struct SSH_Node));
	if ((p_node = ssh_list_search(p_ssh_list, p_auth->host, p_auth->user))) {
		log_write("Found ssh node for %s@%s\n", p_auth->user, p_auth->host);
		if (p_auth->mode == CONN_AUTH_MODE_PROMPT && strcmp(p_auth->password, p_node->password) != 0) {
			strcpy(p_auth->error_s, "Wrong password");
			p_auth->error_code = SSH_ERR_AUTH;
			return (NULL);
		}
		/* check node validity */
		ssh_channel c;
		log_write("Tryng to open a channel on %s@%s\n", p_auth->user, p_auth->host);
		if ((c = ssh_node_open_channel(p_node))) {
			log_debug("Channel successfully opened, close it and return\n");
			ssh_channel_close(c);
			ssh_channel_free(c);
			ssh_node_ref(p_node);
			return (p_node);
		} else
			valid = 0;
		if (!valid) {
			log_write("Not a valid node for to %s@%s, recreate it\n", p_auth->user, p_auth->host);
			node.refcount = p_node->refcount;
			ssh_node_free(p_node);
		}
	}
	log_write("Creating a new ssh node for %s@%s\n", p_auth->user, p_auth->host);
	node.session = ssh_new();
	if (node.session == NULL)
		return (NULL);
	ssh_options_set(node.session, SSH_OPTIONS_HOST, p_auth->host);
	ssh_options_set(node.session, SSH_OPTIONS_USER, p_auth->user);
	ssh_options_set(node.session, SSH_OPTIONS_PORT, &p_auth->port);
	//TODO
	ssh_options_set(node.session, SSH_OPTIONS_TIMEOUT, &timeout);
	rc = ssh_connect(node.session);
	if (rc != SSH_OK) {
		sprintf(p_auth->error_s, "%s", ssh_get_error(node.session));
		p_auth->error_code = SSH_ERR_CONNECT;
		ssh_free(node.session);
		return (NULL);
	}
l_auth:
	/* get authentication methods */
	if (p_auth->mode == CONN_AUTH_MODE_KEY) {
		log_write("Authentication by key\n");
		if (p_auth->identityFile[0])
			ssh_options_set(node.session, SSH_OPTIONS_IDENTITY, p_auth->identityFile);
		rc = ssh_userauth_publickey_auto(node.session, NULL, NULL);
	} else {
		/* auth_none is required before auth_list */
		ssh_userauth_none(node.session, NULL);
		node.auth_methods = ssh_userauth_list(node.session, NULL);
		log_write("auth methods for %s@%s: %d\n", p_auth->user, p_auth->host, node.auth_methods);
		if (node.auth_methods & SSH_AUTH_METHOD_PASSWORD) {
			gsize bytes_read, bytes_written;
			rc = ssh_userauth_password(node.session, NULL,
			                           g_convert(p_auth->password, strlen(p_auth->password),
			                                     "UTF8", "ISO-8859-1", &bytes_read, &bytes_written, &error)
			                          );
			log_write("auth method password returns %d\n", rc);
		} else if (node.auth_methods & SSH_AUTH_METHOD_INTERACTIVE) {
			rc = ssh_userauth_kbdint(node.session, NULL, NULL);
			log_write("auth method interactive: ssh_userauth_kbdint() returns %d\n", rc);
			if (rc == SSH_AUTH_INFO) {
				ssh_userauth_kbdint_setanswer(node.session, 0, p_auth->password);
				rc = ssh_userauth_kbdint(node.session, NULL, NULL);
				log_write("auth method interactive: ssh_userauth_kbdint() returns %d\n", rc);
			} else
				rc = SSH_AUTH_ERROR;
			log_write("auth method interactive returns %d\n", rc);
		} else {
			sprintf(p_auth->error_s, "Unknown authentication method for server %s\n", p_auth->host);
			p_auth->error_code = SSH_ERR_UNKNOWN_AUTH_METHOD;
			ssh_disconnect(node.session);
			ssh_free(node.session);
			return (NULL);
		}
	}
	if (rc != SSH_AUTH_SUCCESS) {
		sprintf(p_auth->error_s, "Authentication error %d: %s", rc, ssh_get_error(node.session));
		if (rc == SSH_AUTH_AGAIN) {
			log_write("%s: SSH_AUTH_AGAIN\n", p_auth->host);
			goto l_auth;
		}
		p_auth->error_code = SSH_ERR_AUTH;
		ssh_disconnect(node.session);
		ssh_free(node.session);
		return (NULL);
	}
	if (p_node)
		memcpy(p_node, &node, sizeof(struct SSH_Node));    /* recreated */
	else
		p_node = ssh_list_append(p_ssh_list, &node);  /* new node */
	strcpy(p_node->user, p_auth->user);
	strcpy(p_node->password, p_auth->password);
	strcpy(p_node->host, p_auth->host);
	p_node->port = p_auth->port;
	p_node->refcount = node.refcount + 1;
	p_node->valid = 1;
	ssh_node_update_time(p_node);
	return (p_node);
}

void ssh_node_free(struct SSH_Node *p_ssh_node)
{
	if (p_ssh_node->session) {
		if (ssh_is_connected(p_ssh_node->session)) {
			log_write("disconnecting node %s@%s\n", p_ssh_node->user, p_ssh_node->host);
			ssh_disconnect(p_ssh_node->session);
		}
		log_debug("releasing node memory\n");
		ssh_free(p_ssh_node->session);
		p_ssh_node->session = NULL;
	}
	p_ssh_node->refcount = 0;
	ssh_node_set_validity(p_ssh_node, 0);
}

void ssh_node_ref(struct SSH_Node *p_ssh_node)
{
	p_ssh_node->refcount ++;
}

void ssh_node_unref(struct SSH_Node *p_ssh_node)
{
	p_ssh_node->refcount --;
	if (p_ssh_node->refcount == 0) {
		log_write("Removing watch file descriptors for %s@%s if any\n", p_ssh_node->user, p_ssh_node->host);
		log_debug("Removing node %s@%s\n", p_ssh_node->user, p_ssh_node->host);
		ssh_node_free(p_ssh_node);
		ssh_list_remove(&globals.ssh_list, p_ssh_node);
	}
}

void ssh_node_set_validity(struct SSH_Node *p_ssh_node, int valid)
{
	p_ssh_node->valid = valid;
}

int ssh_node_get_validity(struct SSH_Node *p_ssh_node)
{
	return (p_ssh_node->valid);
}
ssh_channel ssh_node_open_channel(struct SSH_Node *p_node)
{
	ssh_channel channel;
	int rc;
	log_debug("Opening channel on %s@%s\n", p_node->user, p_node->host);
	if ((channel = ssh_channel_new(p_node->session)) == NULL) {
		ssh_node_set_validity(p_node, 0);
		return (NULL);
	}
	timerStart(2);
	rc = ssh_channel_open_session(channel);
	if (timedOut()) {
		log_debug("Timeout!\n");
		rc = SSH_ERROR;
	}
	timerStop();
	if (rc == SSH_ERROR) {
		log_debug("error: can't open channel on %s@%s\n", p_node->user, p_node->host);
		ssh_channel_free(channel);
		ssh_node_set_validity(p_node, 0);
		return (NULL);
	}
	return (channel);
}

void ssh_node_update_time(struct SSH_Node *p_ssh_node)
{
	if (p_ssh_node)
		p_ssh_node->last = time(NULL);
	log_write("%s: timestamp updated\n", p_ssh_node->host);
}

/* SSH management functions */

void lt_ssh_init(struct SSH_Info *p_ssh)
{
	memset(p_ssh, 0, sizeof(struct SSH_Info));
}
int lt_ssh_connect(struct SSH_Info *p_ssh, struct SSH_List *p_ssh_list, struct SSH_Auth_Data *p_auth)
{
	struct SSH_Node *p_node;
	int rc = 0;
	lockSSH(__func__, TRUE);
	if ((p_node = ssh_node_connect(p_ssh_list, p_auth)) == NULL) {
		sprintf(p_ssh->error_s, "%s", p_auth->error_s);
		rc = p_auth->error_code;
	} else {
		p_ssh->ssh_node = p_node;
		lt_ssh_getenv(p_ssh, "HOME", p_ssh->home);
	}
	lockSSH(__func__, FALSE);
	return (rc);
}

void lt_ssh_disconnect(struct SSH_Info *p_ssh)
{
	log_debug("\n");
	if (p_ssh->ssh_node == NULL)
		return;
	lockSSH(__func__, TRUE);
	log_debug("%s\n", p_ssh->ssh_node->host);
	ssh_node_unref(p_ssh->ssh_node);
	p_ssh->ssh_node = NULL;
	lockSSH(__func__, FALSE);
}

int lt_ssh_is_connected(struct SSH_Info *p_ssh)
{
	int connected = 1;
	log_debug("\n");
	if (p_ssh) {
		if (p_ssh->ssh_node) {
			if (p_ssh->ssh_node->session == NULL)
				connected = 0;
		} else
			connected = 0;
	} else
		connected = 0;
	if (connected && (!ssh_is_connected(p_ssh->ssh_node->session) || !ssh_node_get_validity(p_ssh->ssh_node)))
		connected = 0;
	return (connected);
}

int lt_ssh_getenv(struct SSH_Info *p_ssh, char *variable, char *value)
{
	ssh_channel channel;
	char stmt[256];
	int rc;
	char buffer[256];
	unsigned int nbytes;
	if ((channel = ssh_node_open_channel(p_ssh->ssh_node)) == NULL)
		return (1);
	sprintf(stmt, "echo ${%s}", variable);
	log_debug("%s\n", stmt);
	rc = ssh_channel_request_exec(channel, stmt);
	if (rc != SSH_OK) {
		log_write("Error: can't execute statement: %s\n", stmt);
		ssh_channel_close(channel);
		ssh_channel_free(channel);
		ssh_node_set_validity(p_ssh->ssh_node, 0);
		return (rc);
	}
	strcpy(value, "");
	log_debug("Reading...\n");
	nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
	while (nbytes > 0) {
		log_debug("Read %d bytes\n", nbytes);
		buffer [nbytes] = 0;
		strcat(value, buffer);
		nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
	}
	if (nbytes < 0) {
		log_write("Error: can't execute statement: %s\n", stmt);
		ssh_channel_close(channel);
		ssh_channel_free(channel);
		ssh_node_set_validity(p_ssh->ssh_node, 0);
		return (SSH_ERROR);
	}
	log_debug("Value successfully read\n");
	trim(value);
	log_debug("%s=%s\n", variable, value);
	log_write("Closing channel on %s@%s\n", p_ssh->ssh_node->user, p_ssh->ssh_node->host);
	ssh_channel_send_eof(channel);
	ssh_channel_close(channel);
	ssh_channel_free(channel);
	ssh_node_update_time(p_ssh->ssh_node);
	return (0);
}
int lt_ssh_exec(struct SSH_Info *p_ssh, char *command, char *output, int outlen, char *error, int errlen)
{
	ssh_channel channel;
	int rc;
	char buffer[32];
	unsigned int nbytes;
	////////////////////////////////
	lockSSH(__func__, TRUE);
	if ((channel = ssh_node_open_channel(p_ssh->ssh_node)) == NULL)
		return (1);
	rc = ssh_channel_request_exec(channel, command);
	if (rc != SSH_OK) {
		log_debug("error\n");
	}
	/* Read output */
	strcpy(output, "");
	nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
	while (nbytes > 0) {
		buffer [nbytes] = 0;
		strcat(output, buffer);
		nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
		if (strlen(output) + nbytes > outlen)
			break;
	}
	/* Read error */
	strcpy(error, "");
	nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 1);
	while (nbytes > 0) {
		buffer [nbytes] = 0;
		strcat(error, buffer);
		nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 1);
		//printf ("%d > %d\n", (int) strlen (error) + nbytes, errlen);
		if (strlen(error) + nbytes > errlen)
			break;
	}
	ssh_channel_send_eof(channel);
	ssh_channel_close(channel);
	ssh_channel_free(channel);
	if (rc == SSH_OK) {
		ssh_node_update_time(p_ssh->ssh_node);
		rc = 0;
	}
	////////////////////////////////
	lockSSH(__func__, FALSE);
	return (rc);
}

