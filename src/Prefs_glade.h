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

// Prototypes
static void set_values();
static void get_values();
static void ghost_widgets();
extern "C" gboolean on_prefs_win_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data);


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
			g_signal_connect(GTK_WINDOW(gtk_builder_get_object(builder, "prefs_win")), "delete_event", G_CALLBACK(on_prefs_win_delete_event), nullptr);
			set_values();
		}
	}

	// No UI means no prefs editor
	if (builder == nullptr)
		return startup;

	GtkWindow * prefs_win = GTK_WINDOW(gtk_builder_get_object(builder, "prefs_win"));

	if (startup) {
		gtk_menu_item_set_label(GTK_MENU_ITEM(gtk_builder_get_object(builder, "ok_menu")), "Start");
		gtk_button_set_label(GTK_BUTTON(gtk_builder_get_object(builder, "ok_button")), "Start");
	} else {
		gtk_menu_item_set_label(GTK_MENU_ITEM(gtk_builder_get_object(builder, "ok_menu")), "Continue");
		gtk_button_set_label(GTK_BUTTON(gtk_builder_get_object(builder, "ok_button")), "Continue");
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
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "sprites_on")), prefs->SpritesOn);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "sprite_collisions")), prefs->SpriteCollisions);

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

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "normal_cycles")), prefs->NormalCycles);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "bad_line_cycles")), prefs->BadLineCycles);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "cia_cycles")), prefs->CIACycles);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "floppy_cycles")), prefs->FloppyCycles);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "cia_irq_hack")), prefs->CIAIRQHack);

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
	prefs->SpritesOn = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "sprites_on")));
	prefs->SpriteCollisions = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "sprite_collisions")));

	prefs->SIDType = gtk_combo_box_get_active(GTK_COMBO_BOX(gtk_builder_get_object(builder, "sid_type")));
	prefs->SIDFilters = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "sid_filters")));

	prefs->Joystick1Port = gtk_combo_box_get_active(GTK_COMBO_BOX(gtk_builder_get_object(builder, "joystick1_port")));
	prefs->Joystick2Port = gtk_combo_box_get_active(GTK_COMBO_BOX(gtk_builder_get_object(builder, "joystick2_port")));
	prefs->JoystickSwap = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "joystick_swap")));

	prefs->SkipFrames = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "skip_frames")));
	prefs->LimitSpeed = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "limit_speed")));
	prefs->FastReset = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "fast_reset")));

	prefs->REUSize = gtk_combo_box_get_active(GTK_COMBO_BOX(gtk_builder_get_object(builder, "reu_size")));

	prefs->NormalCycles = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "normal_cycles")));
	prefs->BadLineCycles = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "bad_line_cycles")));
	prefs->CIACycles = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "cia_cycles")));
	prefs->FloppyCycles = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "floppy_cycles")));

	prefs->CIAIRQHack = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "cia_irq_hack")));
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

	ghost_widget("sid_filters", prefs->SIDType != SIDTYPE_DIGITAL);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "sid_filters")), prefs->SIDType == SIDTYPE_DIGITAL ? prefs->SIDFilters : (prefs->SIDType == SIDTYPE_SIDCARD ? true : false));

	ghost_widget("timing_control", IsFrodoSC);
	ghost_widget("advanced_options", IsFrodoSC);
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

extern "C" void on_sid_type_changed(GtkComboBox *box, gpointer user_data)
{
	prefs->SIDType = gtk_combo_box_get_active(box);
	ghost_widgets();
}

extern "C" void on_sid_filters_toggled(GtkToggleButton *button, gpointer user_data)
{
	prefs->SIDFilters = gtk_toggle_button_get_active(button);
}
