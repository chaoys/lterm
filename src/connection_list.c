
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
 * @file connection_list.c
 * @brief
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "connection.h"
#include "connection_list.h"
#include "main.h"
#include "utils.h"

extern Globals globals;
extern Prefs prefs;
extern struct Connection_List conn_list;
extern struct Protocol g_ssh_prot;

void
connection_init (SConnection *pConn)
{
	memset (pConn, 0, sizeof (SConnection) );
}

void
cl_init (struct Connection_List *p_cl)
{
	p_cl->head = NULL;
	p_cl->tail = NULL;
}

void
cl_release_chain (struct Connection *p_head)
{
	if (p_head) {
		cl_release_chain (p_head->next);
		free (p_head);
	}
}

void
cl_release (struct Connection_List *p_cl)
{
	cl_release_chain (p_cl->head);
	p_cl->head = 0;
	p_cl->tail = 0;
}

struct Connection *
cl_append (struct Connection_List *p_cl, struct Connection *p_new)
{
	struct Connection *p_new_decl;
	p_new_decl = (struct Connection *) malloc (sizeof (struct Connection) );
	memset (p_new_decl, 0, sizeof (struct Connection) );
	memcpy (p_new_decl, p_new, sizeof (struct Connection) );
	p_new_decl->next = 0;
	if (p_cl->head == 0) {
		p_cl->head = p_new_decl;
		p_cl->tail = p_new_decl;
	} else {
		p_cl->tail->next = p_new_decl;
		p_cl->tail = p_new_decl;
	}
	return (p_new_decl);
}

struct Connection *
cl_insert_sorted (struct Connection_List *p_cl, struct Connection *p_new)
{
	struct Connection *p_new_decl, *p, *p_prec;
	p_new_decl = (struct Connection *) malloc (sizeof (struct Connection) );
	memset (p_new_decl, 0, sizeof (struct Connection) );
	memcpy (p_new_decl, p_new, sizeof (struct Connection) );
	p_new_decl->next = 0;
	if (p_cl->head == 0) {
		p_cl->head = p_new_decl;
		p_cl->tail = p_new_decl;
	} else {
		p = p_cl->head;
		p_prec = 0;
		while (p) {
			if (strcasecmp (p->name, p_new_decl->name) > 0) {
				break;
			}
			p_prec = p;
			p = p->next;
		}
		if (p == NULL) {         /* last */
			p_cl->tail->next = p_new_decl;
			p_cl->tail = p_new_decl;
		} else {
			if (p == p_cl->head) { /* first */
				p_new_decl->next = p;
				p_cl->head = p_new_decl;
			} else {             /* middle */
				p_prec->next = p_new_decl;
				p_new_decl->next = p;
			}
		}
	}
	return (p_new_decl);
}

struct Connection *
cl_host_search (struct Connection_List *p_cl, char *host, char *skip_this)
{
	struct Connection *p_conn;
	p_conn = p_cl->head;
	while (p_conn) {
		if (!strcmp (p_conn->host, host) ) {
			if (skip_this) {
				if (strcmp (p_conn->name, skip_this) )
					break;
			} else
				break;
		}
		p_conn = p_conn->next;
	}
	return (p_conn);
}

struct Connection *
cl_get_by_index (struct Connection_List *p_cl, int index)
{
	struct Connection *p_conn;
	int i;
	p_conn = (struct Connection *) p_cl->head;
	i = 0;
	while (p_conn) {
		if (i == index)
			break;
		p_conn = p_conn->next;
		i ++;
	}
	return (p_conn);
}

struct Connection *
cl_get_by_name (struct Connection_List *p_cl, char *name)
{
	struct Connection *p_conn;
	p_conn = p_cl->head;
	while (p_conn) {
		if (!strcmp (p_conn->name, name) )
			break;
		p_conn = p_conn->next;
	}
	return (p_conn);
}

void
cl_remove (struct Connection_List *p_cl, char *name)
{
	struct Connection *p_del, *p_prec;
	p_prec = 0;
	p_del = p_cl->head;
	while (p_del) {
		if (!strcmp (p_del->name, name) ) {
			if (p_prec)
				p_prec->next = p_del->next;
			else
				p_cl->head = p_del->next;
			if (p_cl->tail == p_del)
				p_cl->tail = p_prec;
			free (p_del);
			break;
		}
		p_prec = p_del;
		p_del = p_del->next;
	}
}

int
cl_count (struct Connection_List *p_cl)
{
	struct Connection *c;
	int n;
	c = p_cl->head;
	n = 0;
	while (c) {
		n ++;
		c = c->next;
	}
	return (n);
}

void
cl_dump (struct Connection_List *p_cl)
{
	struct Connection *c;
	c = p_cl->head;
	while (c) {
		printf ("%s %s %d\n", c->name, c->host, c->port);
		c = c->next;
	}
}

void
connection_copy (struct Connection *p_dst, struct Connection *p_src)
{
	memcpy (p_dst, p_src, sizeof (struct Connection) );
}

/**
 * connection_fill_from_string() - fills a Connetion struct with values from a connection string
 * @param[in] p_conn pointer connection to be filled
 * @param[in] connection_string string in the format "user/password@connectionname[protocol]"
 * @param[out] dest string buffer receiving the expanded string
 * @return 0 if ok, 1 otherwise
 */
int
connection_fill_from_string (struct Connection *p_conn, char *connection_string)
{
	int i, c, j;
	int step;
	char user[256];
	char password[256];
	char name[256];
	char protocol[256];
	struct Connection *p_conn_saved;
	memset (p_conn, 0, sizeof (struct Connection) );
	strcpy (user, "");
	strcpy (password, "");
	strcpy (name, "");
	strcpy (protocol, "");
	step = 1; /* user */
	j = 0;
	for (i = 0; i < strlen (connection_string); i++) {
		c = connection_string[i];
		switch (step) {
			case 1: /* user */
				if (c == '/' || c == '@') {
					user[j] = 0;
					j = 0;
					if (c == '/')
						step = 2;
					else
						step = 3;
					continue;
				}
				user[j++] = c;
				break;
			case 2: /* password */
				if (c == '@') {
					password[j] = 0;
					j = 0;
					step ++;
					continue;
				}
				password[j++] = c;
				break;
			case 3: /* name */
				if (c == '[') {
					name[j] = 0;
					trim (name);
					j = 0;
					step ++;
					continue;
				}
				name[j++] = c;
				break;
			case 4: /* protocol */
				if (c == ']') {
					protocol[j] = 0;
					j = 0;
					step ++;
					continue;
				}
				protocol[j++] = c;
				break;
		}
	}
	/* add trailing null to last element */
	switch (step) {
		case 1:
			user[j] = 0;
			break;
		case 2:
			password[j] = 0;
			break;
		case 3:
			name[j] = 0;
			break;
		case 4:
			protocol[j] = 0;
			break;
		default:
			break;
	}
	log_debug ("last step %d: %s/%s@%s[%s]\n", step, user, password, name, protocol);
	p_conn_saved = get_connection (&conn_list, name);
	if (p_conn_saved) {
		log_debug ("found: %s\n", p_conn_saved->name);
		connection_copy (p_conn, p_conn_saved);
		strcpy (p_conn->user, user);
		strcpy (p_conn->password, password);
		log_debug ("%s/%s@%s\n", p_conn->user, p_conn->password, p_conn->name);
		return 0;
	} else
		return 2;
}

