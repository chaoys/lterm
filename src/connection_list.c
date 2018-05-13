
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
#include "main.h"

static void free_conn(gpointer data)
{
	free(data);
}
static int namecmp(const void *n1, const void *n2)
{
	return strcasecmp((const char *)n1, (const char *)n2);
}

void
connection_init (Connection *pConn)
{
	memset (pConn, 0, sizeof (Connection) );
}

void
cl_release (GList **p_cl)
{
	g_list_free_full(*p_cl, free_conn);
	*p_cl = NULL;
}

Connection *
cl_append (GList **p_cl, Connection *p_new)
{
	Connection *p_new_decl;
	p_new_decl = (Connection *) malloc (sizeof (Connection) );
	memset (p_new_decl, 0, sizeof (Connection) );
	memcpy (p_new_decl, p_new, sizeof (Connection) );

	*p_cl = g_list_append(*p_cl, p_new_decl);
	return (p_new_decl);
}

void
cl_remove (GList **p_cl, char *name)
{
	GList *res = g_list_find_custom(*p_cl, name, namecmp);
	if (!res) return;
	*p_cl = g_list_remove_link(*p_cl, res);
	g_list_free_full(res, free_conn);
}

Connection *
cl_insert_sorted (GList **p_cl, Connection *p_new)
{
	Connection *p_new_decl;
	p_new_decl = (Connection *) malloc (sizeof (Connection) );
	memset (p_new_decl, 0, sizeof (Connection) );
	memcpy (p_new_decl, p_new, sizeof (Connection) );
	*p_cl = g_list_insert_sorted(*p_cl, p_new_decl, namecmp);
	return (p_new_decl);
}

Connection *
cl_get_by_index (GList *p_cl, int index)
{
	return g_list_nth_data(p_cl, index);
}

Connection *
cl_get_by_name (GList *p_cl, char *name)
{
	GList *res = g_list_find_custom(p_cl, name, namecmp);
	return res ? res->data : NULL;
}

int
cl_count (GList *p_cl)
{
	return g_list_length(p_cl);
}

void
connection_copy (Connection *p_dst, Connection *p_src)
{
	memcpy (p_dst, p_src, sizeof (Connection) );
}

