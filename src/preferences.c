
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
 * @file preferences.c
 * @brief Preferences management
 */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <sys/utsname.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include "main.h"
#include "gui.h"
#include "preferences.h"
#include "config.h"
#include "main.h"
#include "connection.h"
#include "profile.h"

extern Globals globals;
extern Prefs prefs;
extern GtkWidget *main_window;
extern struct ConnectionTab *p_current_connection_tab;
extern struct Profile g_profile;

GtkWidget *dialog_preferences;
GtkWidget *vte_profile;
GtkWidget *fontbutton_terminal;

void
check_use_system_cb (GtkToggleButton *togglebutton, gpointer user_data)
{
	gtk_widget_set_state_flags (fontbutton_terminal,
	                            gtk_toggle_button_get_active (togglebutton) ? GTK_STATE_FLAG_INSENSITIVE : GTK_STATE_FLAG_NORMAL,
	                            TRUE);
}

void
profile_edit (GtkWidget *demo_vte)
{
	GtkBuilder *builder;
	GError *error = NULL;
	GtkWidget *dialog;
	struct Profile new_profile;
	char ui[600];

	builder = gtk_builder_new ();
	sprintf (ui, "%s/profile.glade", globals.data_dir);
	if (gtk_builder_add_from_file (builder, ui, &error) == 0) {
		msgbox_error ("Can't load user interface file:\n%s", error->message);
		return;
	}
	/* Create dialog */
	dialog = gtk_dialog_new_with_buttons
	         (_ ("Edit profile"), NULL,
	          GTK_DIALOG_MODAL,
	          "_Cancel", GTK_RESPONSE_CANCEL,
	          "_Ok", GTK_RESPONSE_OK,
	          NULL);
	gtk_window_set_transient_for (GTK_WINDOW (GTK_DIALOG (dialog) ), GTK_WINDOW (main_window) );
	GtkWidget *notebook1 = GTK_WIDGET (gtk_builder_get_object (builder, "notebook1") );
	/* font */
	fontbutton_terminal = GTK_WIDGET (gtk_builder_get_object (builder, "fontbutton_terminal") );
	gtk_font_button_set_font_name (GTK_FONT_BUTTON (fontbutton_terminal), g_profile.font[0] ? g_profile.font : "Monospace 9");
	GtkWidget *check_use_system = GTK_WIDGET (gtk_builder_get_object (builder, "check_use_system") );
	g_signal_connect (check_use_system, "toggled", G_CALLBACK (check_use_system_cb), NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_use_system), g_profile.font_use_system);
	/* cursor */
	GtkWidget *combo_shape = GTK_WIDGET (gtk_builder_get_object (builder, "combo_shape") );
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo_shape), g_profile.cursor_shape);
	GtkWidget *check_blinking = GTK_WIDGET (gtk_builder_get_object (builder, "check_blinking") );
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_blinking), g_profile.cursor_blinking);
	/* colors */
	GtkWidget *color_fg = GTK_WIDGET (gtk_builder_get_object (builder, "color_fg") );
	GtkWidget *color_bg = GTK_WIDGET (gtk_builder_get_object (builder, "color_bg") );
	GdkRGBA fg, bg;
	gdk_rgba_parse (&fg, g_profile.fg_color);
	gdk_rgba_parse (&bg, g_profile.bg_color);
	gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (color_fg), &fg);
	gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (color_bg), &bg);
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog) ) ), notebook1);
	gtk_widget_show_all (gtk_dialog_get_content_area (GTK_DIALOG (dialog) ) );
	gint result = gtk_dialog_run (GTK_DIALOG (dialog) );
	if (result == GTK_RESPONSE_OK) {
		new_profile.font_use_system = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_use_system) ) ? 1 : 0;
		strcpy (new_profile.font, gtk_font_button_get_font_name (GTK_FONT_BUTTON (fontbutton_terminal) ) );
		new_profile.cursor_shape = gtk_combo_box_get_active (GTK_COMBO_BOX (combo_shape) );
		new_profile.cursor_blinking = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_blinking) ) ? 1 : 0;
		gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (color_fg), &fg);
		strcpy (new_profile.fg_color, gdk_rgba_to_string (&fg) );
		gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (color_bg), &bg);
		strcpy (new_profile.bg_color, gdk_rgba_to_string (&bg) );
		memcpy (&g_profile, &new_profile, sizeof (struct Profile) );
		apply_profile_terminal(demo_vte, &g_profile);
	}
	gtk_widget_destroy (dialog);
	g_object_unref (G_OBJECT (builder) );
}

void
profile_edit_cb (GtkButton *button, gpointer user_data)
{
	profile_edit ((GtkWidget*)user_data);
}

void
radio_ask_cb (GtkToggleButton *togglebutton, gpointer user_data)
{
	gtk_widget_set_state_flags (GTK_WIDGET (user_data), GTK_STATE_FLAG_INSENSITIVE, TRUE);
}

void
radio_dir_cb (GtkToggleButton *togglebutton, gpointer user_data)
{
	gtk_widget_set_state_flags (GTK_WIDGET (user_data), GTK_STATE_FLAG_NORMAL, TRUE);
}

void
show_preferences (void)
{
	GtkBuilder *builder;
	GError *error = NULL;
	GtkWidget *button_ok, *button_cancel;
	GtkWidget *dialog, *notebook;
	GtkWidget *font_entry, *fg_color_entry, *bg_color_entry;
	struct Profile *p_profile;
	char ui[600];
	builder = gtk_builder_new ();
	sprintf (ui, "%s/preferences.glade", globals.data_dir);
	if (gtk_builder_add_from_file (builder, ui, &error) == 0) {
		msgbox_error ("Can't load user interface file:\n%s", error->message);
		return;
	}
	/* Create dialog */
	dialog = gtk_dialog_new_with_buttons
	         (_ ("Preferences"), NULL,
	          GTK_DIALOG_MODAL,
	          "_Cancel", GTK_RESPONSE_CANCEL,
	          "_Ok", GTK_RESPONSE_OK,
	          NULL);
	gtk_window_set_transient_for (GTK_WINDOW (GTK_DIALOG (dialog) ), GTK_WINDOW (main_window) );
	notebook = GTK_WIDGET (gtk_builder_get_object (builder, "notebook1") );
	/* startup */
	GtkWidget *startlocal_check = GTK_WIDGET (gtk_builder_get_object (builder, "check_startlocal") );
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (startlocal_check), prefs.startup_local_shell);
	GtkWidget *startconn_check = GTK_WIDGET (gtk_builder_get_object (builder, "check_connections") );
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (startconn_check), prefs.startup_show_connections);
	/* tabs */
	GtkWidget *tabs_pos_combo = GTK_WIDGET (gtk_builder_get_object (builder, "combo_tabs_pos") );
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (tabs_pos_combo), _ ("Left") );
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (tabs_pos_combo), _ ("Right") );
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (tabs_pos_combo), _ ("Top") );
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (tabs_pos_combo), _ ("Bottom") );
	gtk_combo_box_set_active (GTK_COMBO_BOX (tabs_pos_combo), prefs.tabs_position);
	/* start directory */
	GtkWidget *start_directory_entry = GTK_WIDGET (gtk_builder_get_object (builder, "entry_dir") );
	gtk_entry_set_text (GTK_ENTRY (start_directory_entry), prefs.local_start_directory);
	GtkWidget *tab_alerts_check = GTK_WIDGET (gtk_builder_get_object (builder, "check_tab_alerts") );
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tab_alerts_check), prefs.tab_alerts);
	/* buttons */
	vte_profile = GTK_WIDGET (gtk_builder_get_object (builder, "vte_profile") );
	vte_terminal_set_size (VTE_TERMINAL (vte_profile), 55, 12);
	vte_terminal_feed (VTE_TERMINAL (vte_profile), 
	                   "drwxr-xr-x  24 root root  4096 dic 21 18:14 lib\r\n"
	                   "drwxr-xr-x  13 root root  4096 dic 21 18:23 var\r\n"
	                   "drwxr-xr-x   2 root root 12288 dic 22 16:32 sbin\r\n"
	                   "drwxr-xr-x   2 root root  4096 dic 22 16:36 bin\r\n"
	                   "dr-xr-xr-x 219 root root     0 gen 23 09:25 proc\r\n"
	                   "dr-xr-xr-x  13 root root     0 gen 23 09:25 sys\r\n"
	                   "drwxr-xr-x 150 root root 12288 gen 23 09:25 etc\r\n"
	                   "drwxr-xr-x  18 root root  4260 gen 23 09:25 dev\r\n", -1);
	/* apply current profile */
	apply_profile_terminal(vte_profile, &g_profile);
	GtkWidget *button_edit = GTK_WIDGET (gtk_builder_get_object (builder, "button_edit") );
	g_signal_connect (G_OBJECT (button_edit), "clicked", G_CALLBACK (profile_edit_cb), vte_profile);
	/* mouse */
	GtkWidget *mouse_copy_on_select_check = GTK_WIDGET (gtk_builder_get_object (builder, "check_mouse_copy") );
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mouse_copy_on_select_check), prefs.mouse_copy_on_select);
	GtkWidget *mouse_paste_on_right_button_check = GTK_WIDGET (gtk_builder_get_object (builder, "check_mouse_paste") );
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mouse_paste_on_right_button_check), prefs.mouse_paste_on_right_button);
	GtkWidget *mouse_autohide_check = GTK_WIDGET (gtk_builder_get_object (builder, "check_mouse_hide") );
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mouse_autohide_check), prefs.mouse_autohide);
	/* scrollback */
	GtkWidget *spin_button = GTK_WIDGET (gtk_builder_get_object (builder, "spin_scrollback") );
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_button), prefs.scrollback_lines);
	GtkWidget *scroll_on_keystroke_check = GTK_WIDGET (gtk_builder_get_object (builder, "check_scrollkey") );
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (scroll_on_keystroke_check), prefs.scroll_on_keystroke);
	GtkWidget *scroll_on_output_check = GTK_WIDGET (gtk_builder_get_object (builder, "check_scrolloutput") );
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (scroll_on_output_check), prefs.scroll_on_output);
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog) ) ), notebook);
	gtk_widget_show_all (gtk_dialog_get_content_area (GTK_DIALOG (dialog) ) );
	gint result = gtk_dialog_run (GTK_DIALOG (dialog) );
	if (result == GTK_RESPONSE_OK) {
		prefs.startup_local_shell = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (startlocal_check) ) ? 1 : 0;
		prefs.startup_show_connections = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (startconn_check) ) ? 1 : 0;
		prefs.tabs_position = gtk_combo_box_get_active (GTK_COMBO_BOX (tabs_pos_combo) );
		prefs.tab_alerts = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (tab_alerts_check) ) ? 1 : 0;
		strcpy (prefs.local_start_directory, gtk_entry_get_text (GTK_ENTRY (start_directory_entry) ) );
		prefs.mouse_copy_on_select = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (mouse_copy_on_select_check) ) ? 1 : 0;
		prefs.mouse_paste_on_right_button = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (mouse_paste_on_right_button_check) ) ? 1 : 0;
		prefs.mouse_autohide = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (mouse_autohide_check) ) ? 1 : 0;
		prefs.scrollback_lines = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin_button) );
		prefs.scroll_on_keystroke = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (scroll_on_keystroke_check) ) ? 1 : 0;
		prefs.scroll_on_output = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (scroll_on_output_check) ) ? 1 : 0;
		apply_preferences ();
		update_all_profiles ();
	}
	gtk_widget_destroy (dialog);
	g_object_unref (G_OBJECT (builder) );
}

