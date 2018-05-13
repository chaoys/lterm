
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
 * @file utils.c
 * @brief Implementa funzioni di utilità
 */

#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "utils.h"

void ltrim(char *s)
{
	int i;
	char *pc;
	i = 0;
	pc = &s[i];
	while (*pc != 0 && *pc == ' ')
		pc = &s[++i];
	strcpy(s, pc);
}

void rtrim(char *s)
{
	int i;
	i = strlen(s) - 1;  /* last char */
	while (i >= 0 && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\f'))
		i --;
	/* now whe are on the last good char */
	s[i + 1] = 0;
}

void trim(char *s)
{
	ltrim(s);
	rtrim(s);
}

void list_init(char *list)
{
	strcpy(list, "");
}


int list_count(char *list, char sep)
{
	int i;
	int n;
	n = 0;
	if (strlen(list) > 0) {
		for (i = 0; i < strlen(list); i++) {
			if (list[i] == sep)
				n++;
		}
		n++; /* # elementi = # separatori + 1 */
	}
	return (n);
}

int list_get_nth(char *list, int n, char sep, char *elem)
{
	int n_cur;
	char *pc;
	char *pstart;
	char *pend;
	//char tmp[2048*5];
	char *tmp = NULL;
	strcpy(elem, "");
	pstart = list;
	n_cur = 1;
	while (pstart) {
		if (n_cur == n) {
			tmp = (char *) malloc(strlen(pstart) + 1);
			strcpy(tmp, pstart);
			pend = (char *) strchr(tmp, sep);
			if (pend) {
				*pend = 0;
			}
			strcpy(elem, tmp);
			free(tmp);
			break;
		}
		pc = (char *) strchr(pstart, sep);
		if (pc)
			pstart = pc + 1;
		else
			pstart = 0;
		n_cur++;
	}
	return (pstart ? 1 : 0);
}

char ** splitString(char *str, char *delimiters, int skipNulls, char *quotes, int trailingNull, int *pCount)
{
	char **splitted = NULL;
	int i = 0, k, t, n = 0;
	char *pstart, *tmp = NULL;
	int insideQuotes = 0;
	if (str == NULL)
		return 0;
	while (i < strlen(str)) {
		pstart = &str[i];
		// Get next token
		insideQuotes = 0;
		k = 0;
		t = 0;
		tmp = (char *) malloc(strlen(pstart) + 1);
		memset(tmp, 0, strlen(pstart) + 1);
		while (pstart[k] != 0) {
			// If the first char is a quote go ahead
			if (quotes && k == 0 && strchr(quotes, pstart[k])) {
				insideQuotes = 1; // Enter the quoted string
				k ++;
				continue;
			}
			if (quotes && insideQuotes && strchr(quotes, pstart[k])) {
				insideQuotes = 0; // Exit the quoted string
			}
			if (!insideQuotes && strchr(delimiters, pstart[k])) {
				k ++;
				break;
			}
			if (!quotes || (quotes && !strchr(quotes, pstart[k]))) {
				if (insideQuotes || (!insideQuotes && !strchr(delimiters, pstart[k])))
					tmp[t++] = pstart[k];
			}
			k++;
			if (!insideQuotes && strchr(delimiters, pstart[k])) {
				k++;
				break;
			}
		}
		tmp[t] = 0;
		i += k;
		if (tmp[0] || (tmp[0] == 0 && !skipNulls)) {
			splitted = realloc(splitted, sizeof(char *) * n + 1);
			splitted[n] = (char *) malloc(strlen(tmp) + 1);
			memcpy(splitted[n], tmp, strlen(tmp) + 1);
			free(tmp);
			n ++;
		}
	}
	if (trailingNull) {
		splitted = realloc(splitted, sizeof(char *) * n + 1);
		splitted[n] = 0;
	}
	if (pCount)
		*pCount = n;
	return (splitted);
}

char *password_encode(char *clear_text)
{
	static char secret_text[4096];
	char *p_bin;
	if (strlen(clear_text) == 0)
		return ("");
	p_bin = g_base64_encode((const guchar *)clear_text, strlen(clear_text));
	strcpy(secret_text, p_bin);
	g_free(p_bin);
	return (secret_text);
}

char *password_decode(char *ecrypted_text)
{
	static char clear_text[4096];
	char *p_bin;
	gsize len;
	if (strlen(ecrypted_text) == 0)
		return ("");
	p_bin = (char *) g_base64_decode(ecrypted_text, &len);
	strncpy(clear_text, p_bin, len);
	clear_text[len] = 0;
	g_free(p_bin);
	return (clear_text);
}

