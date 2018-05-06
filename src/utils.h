
/**
 * @file utils.h
 * @brief Definisce le strutture Prefs e Globals, più alcune costanti
 */

#ifndef _UTILS_H
#define _UTILS_H

#include <stdint.h>

#define LOG_MSG_NONE 0
#define LOG_MSG_TIME 1
#define LOG_MSG_SEP1 2

#define MAXLINE 1024
#define MAXBUFLEN   1024

#define DE_UNKNOWN 0
#define DE_GNOME 1
#define DE_KDE 2
#define DE_XFCE 3
#define DE_CINNAMON 4
#define DE_MAC_OS_X 10

#define NVL(a,b) (a) != NULL ? (a) : (b)

void ltrim (char *s);
void rtrim (char *s);
void trim (char *s);
void lower (char *s);
char **splitString (char *str, char *delimiters, int skipNulls, char *quotes, int trailingNull, int *pCount);
int check_command (char *command);

void list_init (char *list);
int list_count (char *list, char sep);
int list_get_nth (char *list, int n, char sep, char *elem);

char *password_encode (char *clear_text);
char *password_decode (char *ecrypted_text);

#endif

