/*
 *  Prefs_glade.h - Global preferences, GTK specific stuff
 *
 *  Frodo Copyright (C) Christian Bauer
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Version.h"

#include "1541d64.h"

#include <gtk/gtk.h>

#include <SDL.h>


// GTK builder object
static GtkBuilder *builder = nullptr;

// Parameter of ShowEditor()
static bool in_startup = true;

// Result of ShowEditor()
static bool result = false;

// Pointer to preferences being edited
static Prefs *prefs = nullptr;

// Prefs file name
static const char *prefs_path = nullptr;

// Dialog for selecting snapshot file
static GtkWidget *snapshot_dialog = nullptr;
static GtkWidget *snapshot_accept_button = nullptr;

// Prototypes
static void set_values();
static void get_values();
static void ghost_widgets();


/*
 *  Show preferences editor (synchronously)
 *  prefs_name points to the file name of the preferences (which may be changed)
 */

bool Prefs::ShowEditor(bool startup, const char *prefs_name)
{
	in_startup = startup;
	prefs = this;
	prefs_path = prefs_name;

	// Load user interface file on startup
	if (startup) {
		builder = gtk_builder_new();
		GError *error = nullptr;
		if (gtk_builder_add_from_file(builder, DATADIR "Frodo.ui", &error) == 0) {
			g_warning("Couldn't load preferences UI definition: %s\nPreferences editor not available.\n", error->message);
			g_object_unref(builder);
			builder = nullptr;
		} else {
			gtk_builder_connect_signals(builder, nullptr);
			set_values();
		}
	}

	// No UI means no prefs editor
	if (builder == nullptr)
		return startup;

	GtkWindow * prefs_win = GTK_WINDOW(gtk_builder_get_object(builder, "prefs_win"));

	if (startup) {
		if (IsFrodoSC) {
			// Remove entire "Advanced" page
			gtk_notebook_remove_page(GTK_NOTEBOOK(gtk_builder_get_object(builder, "tabs")), -1);
		}

		snapshot_dialog = gtk_file_chooser_dialog_new("", prefs_win,
			GTK_FILE_CHOOSER_ACTION_SAVE, "Cancel", GTK_RESPONSE_CANCEL, nullptr
		);
		snapshot_accept_button = gtk_dialog_add_button(GTK_DIALOG(snapshot_dialog), "Save", GTK_RESPONSE_ACCEPT);
		gtk_dialog_set_default_response(GTK_DIALOG(snapshot_dialog), GTK_RESPONSE_ACCEPT);

		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(snapshot_dialog), "Untitled.snap");
		gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(snapshot_dialog), true);

		GtkFileFilter * filter;
		filter = gtk_file_filter_new();
		gtk_file_filter_add_pattern(filter, "*.snap");
		gtk_file_filter_set_name(filter, "Snapshot Files (*.snap)");
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(snapshot_dialog), filter);
		filter = gtk_file_filter_new();
		gtk_file_filter_add_pattern(filter, "*");
		gtk_file_filter_set_name(filter, "All Files (*.*)");
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(snapshot_dialog), filter);

		gtk_menu_item_set_label(GTK_MENU_ITEM(gtk_builder_get_object(builder, "ok_menu")), "Start");
		gtk_button_set_label(GTK_BUTTON(gtk_builder_get_object(builder, "ok_button")), "Start");
		gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "load_snapshot_menu")), false);
		gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "save_snapshot_menu")), false);
	} else {
		gtk_menu_item_set_label(GTK_MENU_ITEM(gtk_builder_get_object(builder, "ok_menu")), "Continue");
		gtk_button_set_label(GTK_BUTTON(gtk_builder_get_object(builder, "ok_button")), "Continue");
		gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "load_snapshot_menu")), true);
		gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "save_snapshot_menu")), true);
	}

	// Run editor
	result = false;

	gtk_window_present(prefs_win);
	gtk_main();

	return result;
}


/*
 *  Set the values of the widgets
 */

static void create_joystick_menu(const char *widget_name)
{
	GtkComboBoxText *w = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, widget_name));

	gtk_combo_box_text_remove_all(w);
	gtk_combo_box_text_append_text(w, "None");

	for (int i = 0; i < SDL_NumJoysticks(); ++i) {
		gtk_combo_box_text_append_text(w, SDL_JoystickNameForIndex(i));
	}
}

static void set_values()
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "emul1541_proc")), prefs->Emul1541Proc);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "map_slash")), prefs->MapSlash);

	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(gtk_builder_get_object(builder, "drive8_path")), prefs->DrivePath[0]);
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(gtk_builder_get_object(builder, "drive9_path")), prefs->DrivePath[1]);
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(gtk_builder_get_object(builder, "drive10_path")), prefs->DrivePath[2]);
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(gtk_builder_get_object(builder, "drive11_path")), prefs->DrivePath[3]);

	gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_builder_get_object(builder, "display_type")), prefs->DisplayType);
	gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_builder_get_object(builder, "scaling_numerator")), prefs->ScalingNumerator - 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "sprites_on")), prefs->SpritesOn);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "sprite_collisions")), prefs->SpriteCollisions);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "show_leds")), prefs->ShowLEDs);

	gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_builder_get_object(builder, "sid_type")), prefs->SIDType);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "sid_filters")), prefs->SIDFilters);

	create_joystick_menu("joystick1_port");
	create_joystick_menu("joystick2_port");

	gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_builder_get_object(builder, "joystick1_port")), prefs->Joystick1Port);
	gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_builder_get_object(builder, "joystick2_port")), prefs->Joystick2Port);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "joystick_swap")), prefs->JoystickSwap);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "skip_frames")), prefs->SkipFrames);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "limit_speed")), prefs->LimitSpeed);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "fast_reset")), prefs->FastReset);

	gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_builder_get_object(builder, "reu_size")), prefs->REUSize);

	if (! IsFrodoSC) {
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "normal_cycles")), prefs->NormalCycles);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "bad_line_cycles")), prefs->BadLineCycles);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "cia_cycles")), prefs->CIACycles);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "floppy_cycles")), prefs->FloppyCycles);

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "cia_irq_hack")), prefs->CIAIRQHack);
	}

	ghost_widgets();
}


/*
 *  Get the values of the widgets
 */

static void get_drive_path(int num, const char *widget_name)
{
	prefs->DrivePath[num][0] = 0;
	gchar *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(gtk_builder_get_object(builder, widget_name)));
	if (path) {
		strncpy(prefs->DrivePath[num], path, 255);
		g_free(path);
	}
	prefs->DrivePath[num][255] = 0;
}

static void get_values()
{
	prefs->Emul1541Proc = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "emul1541_proc")));
	prefs->MapSlash = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "map_slash")));

	get_drive_path(0, "drive8_path");
	get_drive_path(1, "drive9_path");
	get_drive_path(2, "drive10_path");
	get_drive_path(3, "drive11_path");

	prefs->DisplayType = gtk_combo_box_get_active(GTK_COMBO_BOX(gtk_builder_get_object(builder, "display_type")));
	prefs->ScalingNumerator = gtk_combo_box_get_active(GTK_COMBO_BOX(gtk_builder_get_object(builder, "scaling_numerator"))) + 1;
	prefs->ScalingDenominator = 1;  // for now...
	prefs->SpritesOn = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "sprites_on")));
	prefs->SpriteCollisions = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "sprite_collisions")));
	prefs->ShowLEDs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "show_leds")));

	prefs->SIDType = gtk_combo_box_get_active(GTK_COMBO_BOX(gtk_builder_get_object(builder, "sid_type")));
	prefs->SIDFilters = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "sid_filters")));

	prefs->Joystick1Port = gtk_combo_box_get_active(GTK_COMBO_BOX(gtk_builder_get_object(builder, "joystick1_port")));
	prefs->Joystick2Port = gtk_combo_box_get_active(GTK_COMBO_BOX(gtk_builder_get_object(builder, "joystick2_port")));
	prefs->JoystickSwap = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "joystick_swap")));

	prefs->SkipFrames = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "skip_frames")));
	prefs->LimitSpeed = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "limit_speed")));
	prefs->FastReset = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "fast_reset")));

	prefs->REUSize = gtk_combo_box_get_active(GTK_COMBO_BOX(gtk_builder_get_object(builder, "reu_size")));

	if (! IsFrodoSC) {
		prefs->NormalCycles = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "normal_cycles")));
		prefs->BadLineCycles = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "bad_line_cycles")));
		prefs->CIACycles = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "cia_cycles")));
		prefs->FloppyCycles = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "floppy_cycles")));

		prefs->CIAIRQHack = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "cia_irq_hack")));
	}
}


/*
 *  Ghost/unghost widgets
 */

static void ghost_widget(const char *name, bool ghosted)
{
	gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, name)), !ghosted);
}

static void ghost_widgets()
{
	ghost_widget("drive9_path", prefs->Emul1541Proc);
	ghost_widget("drive10_path", prefs->Emul1541Proc);
	ghost_widget("drive11_path", prefs->Emul1541Proc);

	ghost_widget("scaling_numerator", prefs->DisplayType == DISPTYPE_SCREEN);

	ghost_widget("sid_filters", prefs->SIDType != SIDTYPE_DIGITAL);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "sid_filters")), prefs->SIDType == SIDTYPE_DIGITAL ? prefs->SIDFilters : (prefs->SIDType == SIDTYPE_SIDCARD ? true : false));
}


/*
 *  Signal handlers
 */

extern "C" void on_ok_clicked(GtkButton *button, gpointer user_data)
{
	result = true;
	get_values();
	prefs->Save(prefs_path);
	gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(builder, "prefs_win")));
	gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(builder, "about_win")));
	gtk_main_quit();
}

extern "C" void on_quit_clicked(GtkButton *button, gpointer user_data)
{
	result = false;
	gtk_main_quit();
}

extern "C" void on_load_snapshot(GtkMenuItem *menuitem, gpointer user_data)
{
	gtk_window_set_title(GTK_WINDOW(snapshot_dialog), "Frodo: Load Snapshot");
	gtk_file_chooser_set_action(GTK_FILE_CHOOSER(snapshot_dialog), GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_button_set_label(GTK_BUTTON(snapshot_accept_button), "Load");

	bool load_ok = false;
	char * filename = nullptr;

	gint res = gtk_dialog_run(GTK_DIALOG(snapshot_dialog));
	if (res == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(snapshot_dialog));
		load_ok = TheC64->LoadSnapshot(filename);
	}

	gtk_widget_hide(snapshot_dialog);

	if (load_ok) {
		GtkWidget * msg = gtk_message_dialog_new(GTK_WINDOW(gtk_builder_get_object(builder, "prefs_win")),
			GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
			"Snapshot file '%s' loaded.", filename
		);
		gtk_dialog_run(GTK_DIALOG(msg));
		gtk_widget_destroy(msg);
	}

	if (filename) {
		g_free(filename);
	}
}

extern "C" void on_save_snapshot(GtkMenuItem *menuitem, gpointer user_data)
{
	gtk_window_set_title(GTK_WINDOW(snapshot_dialog), "Frodo: Save Snapshot");
	gtk_file_chooser_set_action(GTK_FILE_CHOOSER(snapshot_dialog), GTK_FILE_CHOOSER_ACTION_SAVE);
	gtk_button_set_label(GTK_BUTTON(snapshot_accept_button), "Save");

	bool save_ok = false;
	char * filename = nullptr;

	gint res = gtk_dialog_run(GTK_DIALOG(snapshot_dialog));
	if (res == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(snapshot_dialog));
		save_ok = TheC64->SaveSnapshot(filename);
	}

	gtk_widget_hide(snapshot_dialog);

	if (save_ok) {
		GtkWidget * msg = gtk_message_dialog_new(GTK_WINDOW(gtk_builder_get_object(builder, "prefs_win")),
			GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
			"Snapshot file '%s' saved.", filename
		);
		gtk_dialog_run(GTK_DIALOG(msg));
		gtk_widget_destroy(msg);
	}

	if (filename) {
		g_free(filename);
	}
}

extern "C" void on_about_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkAboutDialog *about_win = GTK_ABOUT_DIALOG(gtk_builder_get_object(builder, "about_win"));
	gtk_about_dialog_set_program_name(about_win, VERSION_STRING);
	gtk_window_present(GTK_WINDOW(about_win));
}

extern "C" void on_emul1541_proc_toggled(GtkToggleButton *button, gpointer user_data)
{
	prefs->Emul1541Proc = gtk_toggle_button_get_active(button);
	ghost_widgets();
}

extern "C" void on_drive8_next_disk_clicked(GtkButton *button, gpointer user_data)
{
	get_drive_path(0, "drive8_path");
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(gtk_builder_get_object(builder, "drive8_path")), NextImageFile(prefs->DrivePath[0]).c_str());
}

extern "C" void on_drive9_next_disk_clicked(GtkButton *button, gpointer user_data)
{
	get_drive_path(1, "drive9_path");
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(gtk_builder_get_object(builder, "drive9_path")), NextImageFile(prefs->DrivePath[1]).c_str());
}

extern "C" void on_drive10_next_disk_clicked(GtkButton *button, gpointer user_data)
{
	get_drive_path(2, "drive10_path");
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(gtk_builder_get_object(builder, "drive10_path")), NextImageFile(prefs->DrivePath[2]).c_str());
}

extern "C" void on_drive11_next_disk_clicked(GtkButton *button, gpointer user_data)
{
	get_drive_path(3, "drive11_path");
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(gtk_builder_get_object(builder, "drive11_path")), NextImageFile(prefs->DrivePath[3]).c_str());
}

extern "C" void on_display_type_changed(GtkComboBox *box, gpointer user_data)
{
	prefs->DisplayType = gtk_combo_box_get_active(box);
	ghost_widgets();
}

extern "C" void on_sid_type_changed(GtkComboBox *box, gpointer user_data)
{
	prefs->SIDType = gtk_combo_box_get_active(box);
	ghost_widgets();
}

extern "C" void on_sid_filters_toggled(GtkToggleButton *button, gpointer user_data)
{
	prefs->SIDFilters = gtk_toggle_button_get_active(button);
}

extern "C" gboolean on_prefs_win_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	// Closing the prefs editor behaves like "Quit" on startup,
	// "Continue" otherwise
	if (in_startup) {
		on_quit_clicked(nullptr, user_data);
	} else {
		on_ok_clicked(nullptr, user_data);
	}

	// Prevent destruction
	return gtk_widget_hide_on_delete(widget);
}

extern "C" gboolean on_about_win_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	// Prevent destruction
	return gtk_widget_hide_on_delete(widget);
}
