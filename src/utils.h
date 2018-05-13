
/**
 * @file utils.h
 * @brief Definisce le strutture Prefs e Globals, più alcune costanti
 */

#ifndef _UTILS_H
#define _UTILS_H

#include <stdint.h>

#define MAXLINE 1024
#define MAXBUFLEN   1024

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

