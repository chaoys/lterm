
#ifndef _TERMINAL_H
#define _TERMINAL_H

#include "gui.h"

int log_on (struct ConnectionTab *p_conn_tab);
char *get_remote_directory_from_vte_title (GtkWidget *vte);
char *get_remote_directory ();

void terminal_write_ex (struct ConnectionTab *p_ct, const char *fmt, ...);
void terminal_write (const char *fmt, ...);
void terminal_write_child_ex (SConnectionTab *pTab, const char *text);
void terminal_write_child (const char *text);
void terminal_set_search_expr (char *expr);
void terminal_find_next ();
void terminal_find_previous ();
int terminal_set_encoding (SConnectionTab *pTab, const char *codeset);
void terminal_set_font_from_string (VteTerminal *vte, const char *font);

#endif

