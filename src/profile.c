
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

Prefs prefs;
Globals globals;

int config_load_string(GKeyFile *kf, char *group, char *key, char *dest, char *default_val)
{
	gchar *str = g_key_file_get_string(kf, group, key, NULL);
	if (str) {
		strcpy(dest, str);
		g_free(str);
	} else {
		strcpy(dest, default_val);
	}
	return 0;
}
int config_load_int(GKeyFile *kf, char *group, char *key, int default_val)
{
	GError *error = NULL;
	gint val = g_key_file_get_integer(kf, group, key, &error);
	if (error) {
		return default_val;
	} else {
		return val;
	}
}
void load_settings(void)
{
	GError *error = NULL;
	GKeyFile *kf = g_key_file_new();
	if (!g_key_file_load_from_file(kf, globals.conf_file, G_KEY_FILE_NONE, &error)) {
		log_debug("Error loading config file: %s\n", error->message);
		// TODO use default
		g_key_file_free(kf);
		return;
	}
	prefs.tabs_position = config_load_int(kf, "general", "tabs_position", GTK_POS_TOP);
	config_load_string(kf, "general", "font_fixed", prefs.font_fixed, DEFAULT_FIXED_FONT);
	config_load_string(kf, "TERMINAL", "extra_word_chars", prefs.extra_word_chars, ":@-./_~?&=%+#");
	prefs.rows = config_load_int(kf, "TERMINAL", "rows", 80);
	prefs.columns = config_load_int(kf, "TERMINAL", "columns", 25);
	config_load_string(kf, "TERMINAL", "character_encoding", prefs.character_encoding, "");
	prefs.scrollback_lines = config_load_int(kf, "TERMINAL", "scrollback_lines", 512);
	prefs.scroll_on_keystroke = config_load_int(kf, "TERMINAL", "scroll_on_keystroke", 1);
	prefs.scroll_on_output = config_load_int(kf, "TERMINAL", "scroll_on_output", 1);
	prefs.mouse_autohide = config_load_int(kf, "MOUSE", "autohide", 1);
	prefs.mouse_copy_on_select = config_load_int(kf, "MOUSE", "copy_on_select", 0);
	prefs.mouse_paste_on_right_button = config_load_int(kf, "MOUSE", "paste_on_right_button", 0);
	prefs.w = config_load_int(kf, "GUI", "w", 640);
	prefs.h = config_load_int(kf, "GUI", "h", 480);
	prefs.maximize = config_load_int(kf, "GUI", "maximize", 0);
	prefs.tab_alerts = config_load_int(kf, "GUI", "tab_alerts", 1);
	config_load_string(kf, "GUI", "tab_status_changed_color", prefs.tab_status_changed_color, "blue");
	config_load_string(kf, "GUI", "tab_status_disconnected_color", prefs.tab_status_disconnected_color, "#707070");
	config_load_string(kf, "GUI", "tab_status_disconnected_alert_color", prefs.tab_status_disconnected_alert_color, "darkred");

	g_key_file_free(kf);
}

void save_settings(void)
{
	GError *error = NULL;
	GKeyFile *kf = g_key_file_new();

	g_key_file_set_string(kf, "general", "package_version", VERSION);
	g_key_file_set_integer(kf, "general", "tabs_position", prefs.tabs_position);
	g_key_file_set_string(kf, "general", "font_fixed", prefs.font_fixed);
	g_key_file_set_integer(kf, "TERMINAL", "scrollback_lines", prefs.scrollback_lines);
	g_key_file_set_integer(kf, "TERMINAL", "scroll_on_keystroke", prefs.scroll_on_keystroke);
	g_key_file_set_integer(kf, "TERMINAL", "scroll_on_output", prefs.scroll_on_output);
	g_key_file_set_integer(kf, "TERMINAL", "rows", prefs.rows);
	g_key_file_set_integer(kf, "TERMINAL", "columns", prefs.columns);
	g_key_file_set_integer(kf, "MOUSE", "autohide", prefs.mouse_autohide);
	g_key_file_set_integer(kf, "MOUSE", "copy_on_select", prefs.mouse_copy_on_select);
	g_key_file_set_integer(kf, "MOUSE", "paste_on_right_button", prefs.mouse_paste_on_right_button);
	g_key_file_set_integer(kf, "GUI", "maximize", prefs.maximize);
	g_key_file_set_integer(kf, "GUI", "w", prefs.w);
	g_key_file_set_integer(kf, "GUI", "h", prefs.h);
	g_key_file_set_integer(kf, "GUI", "tab_alerts", prefs.tab_alerts);

	if (!g_key_file_save_to_file(kf, globals.conf_file, &error)) {
		log_debug("Error saving config file: %s\n", error->message);
		log_write("Error saving config file: %s\n", error->message);
	}
	g_key_file_free(kf);
}
int load_profile(struct Profile *pf, char *filename)
{
	char *xml;
	char line[2048];
	char tmp_s[32];
	FILE *fp;
	/* put xml content into a string */
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
	/* parse the xml document */
	XML xmldoc;
	XMLNode *node, *child, *node_2;
	xml_parse(xml, &xmldoc);
	if (xmldoc.error.code) {
		log_write("%s\n", xmldoc.error.message);
		return 1;
	}
	if (strcmp(xmldoc.cur_root->name, "lterm-profiles")) {
		log_write("[%s] can't find root node: lterm-profiles\n", __func__);
		return 2;
	}
	memset(pf, 0, sizeof(struct Profile));
	node = xmldoc.cur_root->children;
	if (!strcmp(node->name, "profile")) {
		if ((child = xml_node_get_child(node, "fg-color")))
			strcpy(pf->fg_color, NVL(xml_node_get_value(child), ""));
		if ((node_2 = xml_node_get_child(node, "fonts"))) {
			if ((child = xml_node_get_child(node_2, "use-system"))) {
				strcpy(tmp_s, NVL(xml_node_get_value(child), ""));
				if (tmp_s[0])
					pf->font_use_system = atoi(tmp_s);
			}
			if ((child = xml_node_get_child(node_2, "font")))
				strcpy(pf->font, NVL(xml_node_get_value(child), ""));
		}
		if ((node_2 = xml_node_get_child(node, "background"))) {
			if ((child = xml_node_get_child(node_2, "color")))
				strcpy(pf->bg_color, NVL(xml_node_get_value(child), ""));
			if ((child = xml_node_get_child(node_2, "alpha"))) {
				strcpy(tmp_s, NVL(xml_node_get_value(child), ""));
				if (tmp_s[0])
					pf->alpha = atof(tmp_s);
			}
		}
		if ((node_2 = xml_node_get_child(node, "cursor"))) {
			if ((child = xml_node_get_child(node_2, "shape"))) {
				strcpy(tmp_s, NVL(xml_node_get_value(child), "0"));
				if (tmp_s[0])
					pf->cursor_shape = atoi(tmp_s);
			}
			if ((child = xml_node_get_child(node_2, "blinking"))) {
				strcpy(tmp_s, NVL(xml_node_get_value(child), "1"));
				if (tmp_s[0])
					pf->cursor_blinking = atoi(tmp_s);
			}
		}
	}
	xml_free(&xmldoc);
	free(xml);
	return 0;
}

int save_profile(struct Profile *pf, char *filename)
{
	FILE *fp;
	fp = fopen(filename, "w");
	if (fp == NULL)
		return (1);
	fprintf(fp,
	        "<?xml version = '1.0'?>\n"
	        "<!DOCTYPE lterm-profiles>\n"
	        "<lterm-profiles>\n");
	fprintf(fp, "  <profile>\n"
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
	fprintf(fp, "</lterm-profiles>\n");
	fclose(fp);
	return (0);
}

void profile_create_default(struct Profile *pf)
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
