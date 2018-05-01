
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
 * @file profile.c
 * @brief Reads/writes configuration parameters
 */

#include <regex.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "main.h"
#include "profile.h"
#include "utils.h"
#include "xml.h"

/**
 * profile_get_name_param() - extract parameter name and value from a line in the form "param = value"
 * @param[in] line entire line
 * @param[in] param_read buffer receiving the name of parameter
 * @param[out] value_read buffer receiving the assigned value
 * @return 0 if successfull, REG_NOMATCH (defined in <regex.h>) if line is not an assignment
 */
int
profile_get_name_param (char *line, char *param_read, char *value_read)
{
	int m;
	regex_t reg;
	regmatch_t pmatch[50];
	m = regcomp (&reg, "([a-zA-Z_]{1}[a-zA-Z_0-9]*)[ ]*=[ ]*(.*)", REG_EXTENDED | REG_ICASE | REG_NEWLINE);
	m = regexec (&reg, line, 50, pmatch, 0);
	if (m != REG_NOMATCH) {
		/* Get parameter name */
		memcpy (param_read, &line[pmatch[1].rm_so], pmatch[1].rm_eo - pmatch[1].rm_so);
		param_read[pmatch[1].rm_eo - pmatch[1].rm_so] = 0;
		/* Get value */
		memcpy (value_read, &line[pmatch[2].rm_so], pmatch[2].rm_eo - pmatch[2].rm_so);
		value_read[pmatch[2].rm_eo - pmatch[2].rm_so] = 0;
	} else {
		strcpy (param_read, "");
		strcpy (value_read, "");
	}
	regfree (&reg);
	return (m);
}

int
profile_load_string (char *profile_file, char *section, char *param, char *dest, char *default_val)
{
	char line[1024], param_read[64], value_read[1024];
	FILE *fp;
	char *pc, sec[128];
	int section_found;
	int rc;
	strcpy (dest, default_val);
	sprintf (sec, "[%s]", section);
	fp = fopen (profile_file, "r");
	if (fp == NULL) {
		//printf ("%s : uso default\n", param);
		return (PROFILE_FILE_NOT_FOUND);
	}
	/* parameter = value */
	rc = PROFILE_PARAMETER_NOT_FOUND;
	section_found = 0;
	while (fgets (line, 1024, fp) != 0) {
		line[strlen (line) - 1] = 0;
		pc = (char *) strrchr (line, ';');
		if (pc)
			*pc = '\0';
		/* seek section */
		if (!section_found) {
			if (!strncasecmp (sec, line, strlen (sec) ) )
				section_found = 1;
			continue;
		}
		/* if reached next section then exit */
		if (section_found && line[0] == '[') {
			rc = PROFILE_PARAMETER_NOT_FOUND;
			break;
		}
		/* check parameter */
		profile_get_name_param (line, param_read, value_read);
		if (!strcasecmp (param_read, param) ) {
			strcpy (dest, value_read);
			rc = PROFILE_OK;
			break;
		}
	}
	fclose (fp);
	return (rc);
}

int
profile_load_int (char *profile_file, char *section, char *param,
                  int default_val)
{
	int ret;
	char value_str[128];
	ret = default_val;
	profile_load_string (profile_file, section, param, value_str, "not found");
	if (strcmp (value_str, "not found") )
		ret = atoi (value_str);
	return (ret);
}

int
profile_modify_string (int operation, char *profile_file, char *section, char *param, char *value)
{
	char line[1024], line_copy[1024], param_read[64];
	int m;
	FILE *fp, *fp_tmp;
	regex_t reg;
	regmatch_t pmatch[50];
	char *pc, sec[128];
	int section_found;
	char tmp_file[256];
	int saved;
	char *pl;
	saved = 0;
	sprintf (sec, "[%s]", section);
	sprintf (tmp_file, "%s_tmp", profile_file);
	fp = fopen (profile_file, "r");
	if (fp == NULL) {
		/* first execution, create an empty file */
		fp = fopen (profile_file, "w");
		if (fp == NULL)
			return 1;
		else {
			fclose (fp);
			fp = fopen (profile_file, "r");
		}
	}
	fp_tmp = fopen (tmp_file, "w");
	if (fp_tmp == NULL) {
		fclose (fp);
		return 2;
	}
	/* parameter = value */
	m = regcomp (&reg, "([a-zA-Z_]{1}[a-zA-Z_0-9]*)[ ]*=[ ]*(.*)", REG_EXTENDED | REG_ICASE | REG_NEWLINE);
	/* find the section */
	section_found = 0;
	while (!section_found) {
		pl = fgets (line, 1024, fp);
		if (pl == 0) {
			//fputs ("\n", fp_tmp);
			break;
		}
		strcpy (line_copy, line);
		/* exclude new line */
		if (line[strlen (line) - 1] == 0x0a)
			line[strlen (line) - 1] = 0;
		/* write line and go on */
		fprintf (fp_tmp, "%s\n", line);
		if (!strncasecmp (sec, line, strlen (sec) ) ) { /* section found */
			section_found = 1;
		}
	}
	/* if section not found the create it */
	if (!section_found) {
		//fprintf (fp_tmp, "\n%s\n", sec);
		fprintf (fp_tmp, "\n[%s]\n", section);
		section_found = 1;
	}
	/* scan the target section */
	while (section_found && fgets (line, 1024, fp) != 0) {
		strcpy (line_copy, line);
		line[strlen (line) - 1] = 0;
		/* printf ("profile_save_string () : while : %s\n", line); */
		/* if a new section is starting but parameter not saved yet, then append here */
		if (line_copy[0] == '[') {
			if (operation == PROFILE_SAVE && !saved) {
				fprintf (fp_tmp, "%s = %s\n", param, value);
			}
			saved = 1;
			/* copy other lines */
			fputs (line_copy, fp_tmp);
		} else { /* we are in the target section */
			if (!saved) {
				pc = (char *) strrchr (line, ';');
				if (pc)
					*pc = '\0';
				m = regexec (&reg, line, 50, pmatch, 0);
				if (m != REG_NOMATCH) {
					/* get parameter name */
					memcpy (param_read, &line[pmatch[1].rm_so], pmatch[1].rm_eo - pmatch[1].rm_so);
					param_read[pmatch[1].rm_eo - pmatch[1].rm_so] = 0;
					if (!saved && !strcasecmp (param_read, param) ) { /* found old value */
						/* if saving then write, if deleting do nothing */
						if (operation == PROFILE_SAVE) {
							fprintf (fp_tmp, "%s = %s\n", param_read, value);
						}
						saved = 1;
					} else {
						/* copy other lines */
						fputs (line_copy, fp_tmp);
					}
				}
			} else { /* already saved, copy other lines */
				fputs (line_copy, fp_tmp);
			}
		} /* in the section */
	} /* while */
	/* if saving but value has not been set above, the create section and parameter now */
	if (operation == PROFILE_SAVE && !saved) {
		if (!section_found)
			fprintf (fp_tmp, "\n[%s]\n", section);
		fprintf (fp_tmp, "%s = %s\n", param, value);
	}
	fclose (fp_tmp);
	fclose (fp);
	rename (tmp_file, profile_file);
	remove (tmp_file);
	regfree (&reg);
	return 0;
}

int
profile_modify_int (int operation, char *profile_file, char *section, char *param, int value)
{
	char s[256];
	sprintf (s, "%d", value);
	return (profile_modify_string (operation, profile_file, section, param, s) );
}

int
profile_delete_section (char *profile_file, char *section)
{
	char line[1024];
	FILE *fp, *fp_tmp;
	char sec[128];
	int position;
	char tmp_file[256];
	char *pl;
	sprintf (sec, "[%s]", section);
	sprintf (tmp_file, "%s_del_tmp", profile_file);
	fp = fopen (profile_file, "r");
	if (fp == NULL) {
		/* first execution, create an empty file */
		fp = fopen (profile_file, "w");
		if (fp == NULL)
			return 1;
		else {
			fclose (fp);
			fp = fopen (profile_file, "r");
		}
	}
	fp_tmp = fopen (tmp_file, "w");
	if (fp_tmp == NULL) {
		fclose (fp);
		return 2;
	}
	position = PROFILE_BEFORE_SECTION;
	while ((pl = fgets (line, 1024, fp))) {
		/* check section switch */
		if (position == PROFILE_IN_SECTION) {
			if (line[0] == '[')
				position = PROFILE_AFTER_SECTION;
		} else {
			if (!strncasecmp (sec, line, strlen (sec) ) ) /* section found */
				position = PROFILE_IN_SECTION;
		}
		/* exclude target section */
		if (position != PROFILE_IN_SECTION) {
			/* exclude new line */
			if (line[strlen (line) - 1] == 0x0a)
				line[strlen (line) - 1] = 0;
			/* write line and go on */
			if (strlen (line) )
				fprintf (fp_tmp, "%s\n", line);
		}
	}
	fclose (fp_tmp);
	fclose (fp);
	rename (tmp_file, profile_file);
	remove (tmp_file);
	return 0;
}

int
load_profile (struct Profile *pf, char *filename)
{
	char *xml;
	char line[2048];
	char tmp_s[32];
	FILE *fp;
	/* put xml content into a string */
	fp = fopen (filename, "r");
	if (fp == NULL)
		return (1);
	xml = (char *) malloc (2048);
	strcpy (xml, "");
	while (fgets (line, 1024, fp) != 0) {
		if (strlen (xml) + strlen (line) > sizeof (xml) )
			xml = (char *) realloc (xml, strlen (xml) + strlen (line) + 1);
		strcat (xml, line);
	}
	fclose (fp);
	/* parse the xml document */
	XML xmldoc;
	XMLNode *node, *child, *node_2;
	xml_parse (xml, &xmldoc);
	if (xmldoc.error.code) {
		log_write ("%s\n", xmldoc.error.message);
		return 1;
	}
	if (strcmp (xmldoc.cur_root->name, "lterm-profiles") ) {
		log_write ("[%s] can't find root node: lterm-profiles\n", __func__);
		return 2;
	}
	memset (pf, 0, sizeof (struct Profile) );
	node = xmldoc.cur_root->children;
	if (!strcmp (node->name, "profile") ) {
		if ((child = xml_node_get_child (node, "fg-color")))
			strcpy (pf->fg_color, NVL (xml_node_get_value (child), "") );
		if ((node_2 = xml_node_get_child (node, "fonts"))) {
			if ((child = xml_node_get_child (node_2, "use-system"))) {
				strcpy (tmp_s, NVL (xml_node_get_value (child), "") );
				if (tmp_s[0])
					pf->font_use_system = atoi (tmp_s);
			}
			if ((child = xml_node_get_child (node_2, "font")))
				strcpy (pf->font, NVL (xml_node_get_value (child), "") );
		}
		if ((node_2 = xml_node_get_child (node, "background"))) {
			if ((child = xml_node_get_child (node_2, "color")))
				strcpy (pf->bg_color, NVL (xml_node_get_value (child), "") );
			if ((child = xml_node_get_child (node_2, "alpha"))) {
				strcpy (tmp_s, NVL (xml_node_get_value (child), "") );
				if (tmp_s[0])
					pf->alpha = atof (tmp_s);
			}
		}
		if ((node_2 = xml_node_get_child (node, "cursor"))) {
			if ((child = xml_node_get_child (node_2, "shape"))) {
				strcpy (tmp_s, NVL (xml_node_get_value (child), "0") );
				if (tmp_s[0])
					pf->cursor_shape = atoi (tmp_s);
			}
			if ((child = xml_node_get_child (node_2, "blinking"))) {
				strcpy (tmp_s, NVL (xml_node_get_value (child), "1") );
				if (tmp_s[0])
					pf->cursor_blinking = atoi (tmp_s);
			}
		}
	}
	xml_free (&xmldoc);
	free (xml);
	return 0;
}

int
save_profile (struct Profile *pf, char *filename)
{
	FILE *fp;
	fp = fopen (filename, "w");
	if (fp == NULL)
		return (1);
	fprintf (fp,
	         "<?xml version = '1.0'?>\n"
	         "<!DOCTYPE lterm-profiles>\n"
	         "<lterm-profiles>\n");
	fprintf (fp, "  <profile>\n"
			 "    <fonts>\n"
			 "      <use-system>%d</use-system>\n"
			 "      <font>%s</font>\n"
			 "    </fonts>\n"
			 "    <fg-color>%s</fg-color>\n"
			 "    <background>\n"
			 "      <color>%s</color>\n"
			 "      <alpha>%.2f</alpha>\n"
			 "    </background>\n"
			 "    <cursor>\n"
			 "      <shape>%d</shape>\n"
			 "      <blinking>%d</blinking>\n"
			 "    </cursor>\n"
			 "  </profile>\n",
			 pf->font_use_system, pf->font, pf->fg_color,
			 pf->bg_color, pf->alpha,
			 pf->cursor_shape, pf->cursor_blinking);
	fprintf (fp, "</lterm-profiles>\n");
	fclose (fp);
	return (0);
}

void
profile_create_default (struct Profile *pf)
{
	struct Profile default_profile = {
		1, /* Use system font for terminal */
		"", /* font */
		"black", /* bg */
		"light gray", /* fg */
		1.0, /* transparent */
		0, /* cursor_shape (block) */
		1, /* cursor_blinking */
	};
	memcpy(pf, &default_profile, sizeof(struct Profile));
}
