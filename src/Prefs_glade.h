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
#include "main.h"
#include "SAM.h"

#include <gtk/gtk.h>

#include <SDL.h>

#include <format>


// GTK builder object
static GtkBuilder *builder = nullptr;

// Parameter of ShowEditor()
static bool in_startup = true;

// Result of ShowEditor()
static bool result = false;

// Pointer to preferences being edited
static Prefs *prefs = nullptr;

// Main settings window
static GtkWindow * prefs_win = nullptr;

// Dialog for selecting snapshot file
static GtkWidget *snapshot_dialog = nullptr;
static GtkWidget *snapshot_accept_button = nullptr;

// Dialog for creating disk image file
static GtkWidget *create_image_dialog = nullptr;

// SAM text view and buffer
static GtkTextView *sam_view = nullptr;
static GtkTextBuffer *sam_buffer = nullptr;
static GtkTextMark *sam_input_start = nullptr;

// Prototypes
static void set_values();
static void get_values();
static void ghost_widgets();
static void write_sam_output(std::string s, bool error = false);

// Shortcuts window definition
static const char * shortcuts_win_ui =
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
	"<interface domain=\"marker\">"
	"<object class=\"GtkShortcutsWindow\" id=\"shortcuts_win\">"
      "<property name=\"modal\">1</property>"
	  "<child>"
	    "<object class=\"GtkShortcutsSection\">"
	      "<property name=\"visible\">1</property>"
	      "<property name=\"section-name\">Keyboard Shortcuts</property>"
	      "<child>"
	        "<object class=\"GtkShortcutsGroup\">"
	          "<property name=\"visible\">1</property>"
	          "<property name=\"title\">Emulator Window</property>"
	          "<child>"
	            "<object class=\"GtkShortcutsShortcut\">"
	              "<property name=\"visible\">1</property>"
	              "<property name=\"accelerator\">F10</property>"
	              "<property name=\"title\">Open settings window</property>"
	            "</object>"
	          "</child>"
	          "<child>"
	            "<object class=\"GtkShortcutsShortcut\">"
	              "<property name=\"visible\">1</property>"
	              "<property name=\"accelerator\">F12</property>"
	              "<property name=\"title\">Reset C64</property>"
	            "</object>"
	          "</child>"
	          "<child>"
	            "<object class=\"GtkShortcutsShortcut\">"
	              "<property name=\"visible\">1</property>"
	              "<property name=\"accelerator\">&lt;shift&gt;F12</property>"
	              "<property name=\"title\">Reset C64 and clear memory</property>"
	            "</object>"
	          "</child>"
	          "<child>"
	            "<object class=\"GtkShortcutsShortcut\">"
	              "<property name=\"visible\">1</property>"
	              "<property name=\"accelerator\">KP_Enter</property>"
	              "<property name=\"title\">Toggle fullscreen mode</property>"
	            "</object>"
	          "</child>"
	          "<child>"
	            "<object class=\"GtkShortcutsShortcut\">"
	              "<property name=\"visible\">1</property>"
	              "<property name=\"accelerator\">KP_Subtract</property>"
	              "<property name=\"title\">Rewind</property>"
	            "</object>"
	          "</child>"
	          "<child>"
	            "<object class=\"GtkShortcutsShortcut\">"
	              "<property name=\"visible\">1</property>"
	              "<property name=\"accelerator\">KP_Add</property>"
	              "<property name=\"title\">Fast-forward</property>"
	            "</object>"
	          "</child>"
	        "</object>"
	      "</child>"
	      "<child>"
	        "<object class=\"GtkShortcutsGroup\">"
	          "<property name=\"visible\">1</property>"
	          "<property name=\"title\">C64 Keyboard</property>"
	          "<child>"
	            "<object class=\"GtkShortcutsShortcut\">"
	              "<property name=\"visible\">1</property>"
	              "<property name=\"accelerator\">Escape</property>"
	              "<property name=\"title\">RUN/STOP</property>"
	            "</object>"
	          "</child>"
	          "<child>"
	            "<object class=\"GtkShortcutsShortcut\">"
	              "<property name=\"visible\">1</property>"
	              "<property name=\"accelerator\">F11</property>"
	              "<property name=\"title\">RESTORE</property>"
	            "</object>"
	          "</child>"
	          "<child>"
	            "<object class=\"GtkShortcutsShortcut\">"
	              "<property name=\"visible\">1</property>"
	              "<property name=\"accelerator\">&lt;alt&gt;</property>"
	              "<property name=\"title\">C=</property>"
	            "</object>"
	          "</child>"
	          "<child>"
	            "<object class=\"GtkShortcutsShortcut\">"
	              "<property name=\"visible\">1</property>"
	              "<property name=\"accelerator\">BackSpace</property>"
	              "<property name=\"title\">INS/DEL</property>"
	            "</object>"
	          "</child>"
	          "<child>"
	            "<object class=\"GtkShortcutsShortcut\">"
	              "<property name=\"visible\">1</property>"
	              "<property name=\"accelerator\">Home</property>"
	              "<property name=\"title\">CLR/HOME</property>"
	            "</object>"
	          "</child>"
	          "<child>"
	            "<object class=\"GtkShortcutsShortcut\">"
	              "<property name=\"visible\">1</property>"
	              "<property name=\"accelerator\">End</property>"
	              "<property name=\"title\">£</property>"
	            "</object>"
	          "</child>"
	          "<child>"
	            "<object class=\"GtkShortcutsShortcut\">"
	              "<property name=\"visible\">1</property>"
	              "<property name=\"accelerator\">Page_Up</property>"
	              "<property name=\"title\">↑</property>"
	            "</object>"
	          "</child>"
	          "<child>"
	            "<object class=\"GtkShortcutsShortcut\">"
	              "<property name=\"visible\">1</property>"
	              "<property name=\"accelerator\">Page_Down</property>"
	              "<property name=\"title\">=</property>"
	            "</object>"
	          "</child>"
	        "</object>"
	      "</child>"
	    "</object>"
	  "</child>"
	"</object>"
	"</interface>";


/*
 *  Show preferences editor (synchronously)
 *  prefs_path is the preferences file name
 *  snapshot_path is the default directory for snapshots
 */

bool Prefs::ShowEditor(bool startup, fs::path prefs_path, fs::path snapshot_path)
{
	in_startup = startup;
	prefs = this;

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
		}
	}

	// No UI means no prefs editor
	if (builder == nullptr)
		return startup;

	prefs_win = GTK_WINDOW(gtk_builder_get_object(builder, "prefs_win"));

	if (startup) {
		if (IsFrodoSC) {
			// Remove entire "Advanced" page
			gtk_notebook_remove_page(GTK_NOTEBOOK(gtk_builder_get_object(builder, "tabs")), -1);
		}

		// Create dialog for loading/saving snapshot files
		snapshot_dialog = gtk_file_chooser_dialog_new("", prefs_win,
			GTK_FILE_CHOOSER_ACTION_SAVE, "Cancel", GTK_RESPONSE_CANCEL, nullptr
		);
		snapshot_accept_button = gtk_dialog_add_button(GTK_DIALOG(snapshot_dialog), "Save", GTK_RESPONSE_ACCEPT);
		gtk_dialog_set_default_response(GTK_DIALOG(snapshot_dialog), GTK_RESPONSE_ACCEPT);

		gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(snapshot_dialog), snapshot_path.c_str(), nullptr);
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(snapshot_dialog), snapshot_path.c_str());
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

		// Create dialog for creating blank disk image file
		create_image_dialog = gtk_file_chooser_dialog_new("Create Disk Image File", prefs_win,
			GTK_FILE_CHOOSER_ACTION_SAVE, "Cancel", GTK_RESPONSE_CANCEL, "Save", GTK_RESPONSE_ACCEPT, nullptr
		);

		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(create_image_dialog), "Untitled.d64");
		gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(create_image_dialog), true);

		filter = gtk_file_filter_new();
		gtk_file_filter_add_pattern(filter, "*.d64");
		gtk_file_filter_set_name(filter, "C64 Disk Image Files (*.d64)");
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(create_image_dialog), filter);
		filter = gtk_file_filter_new();
		gtk_file_filter_add_pattern(filter, "*");
		gtk_file_filter_set_name(filter, "All Files (*.*)");
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(create_image_dialog), filter);

		// Set up SAM view
		sam_view = GTK_TEXT_VIEW(gtk_builder_get_object(builder, "sam_view"));
		sam_buffer = gtk_text_view_get_buffer(sam_view);

		gtk_text_buffer_create_tag(sam_buffer, "protected", "editable", false, nullptr);
		gtk_text_buffer_create_tag(sam_buffer, "error", "foreground", "red", nullptr);

		GtkTextIter start;
		gtk_text_buffer_get_start_iter(sam_buffer, &start);
		sam_input_start = gtk_text_buffer_create_mark(sam_buffer, "input_start", &start, true);

		write_sam_output(SAM_GetStartupMessage());
		write_sam_output(SAM_GetPrompt());

		// Adjust menus for startup
		gtk_menu_item_set_label(GTK_MENU_ITEM(gtk_builder_get_object(builder, "ok_menu")), "Start");
		gtk_button_set_label(GTK_BUTTON(gtk_builder_get_object(builder, "ok_button")), "Start");
		gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "load_snapshot_menu")), false);
		gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "save_snapshot_menu")), false);
		gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "sam_menu")), false);

	} else {

		// Adjust menus for running emulation
		gtk_menu_item_set_label(GTK_MENU_ITEM(gtk_builder_get_object(builder, "ok_menu")), "Continue");
		gtk_button_set_label(GTK_BUTTON(gtk_builder_get_object(builder, "ok_button")), "Continue");
		gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "load_snapshot_menu")), true);
		gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "save_snapshot_menu")), true);
		gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "sam_menu")), true);

		SAM_GetState(TheC64);
	}

	// Run editor
	result = false;
	set_values();

	gtk_window_present(prefs_win);
	gtk_main();

	// Save preferences if "Start"/"Continue" clicked
	if (result) {
		get_values();
		prefs->Save(prefs_path);

		if (! startup) {
			SAM_SetState(TheC64);
		}
	}

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

	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(gtk_builder_get_object(builder, "drive8_path")), prefs->DrivePath[0].c_str());
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(gtk_builder_get_object(builder, "drive9_path")), prefs->DrivePath[1].c_str());
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(gtk_builder_get_object(builder, "drive10_path")), prefs->DrivePath[2].c_str());
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(gtk_builder_get_object(builder, "drive11_path")), prefs->DrivePath[3].c_str());

	gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_builder_get_object(builder, "display_type")), prefs->DisplayType);
	gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_builder_get_object(builder, "scaling_numerator")), prefs->ScalingNumerator - 1);
	gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_builder_get_object(builder, "palette")), prefs->Palette);
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
	prefs->DrivePath[num].clear();
	gchar *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(gtk_builder_get_object(builder, widget_name)));
	if (path) {
		prefs->DrivePath[num] = path;
		g_free(path);
	}
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
	prefs->Palette = gtk_combo_box_get_active(GTK_COMBO_BOX(gtk_builder_get_object(builder, "palette")));
	prefs->SpritesOn = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "sprites_on")));
	prefs->SpriteCollisions = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "sprite_collisions")));
	prefs->ShowLEDs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "show_leds")));

	prefs->SIDType = gtk_combo_box_get_active(GTK_COMBO_BOX(gtk_builder_get_object(builder, "sid_type")));
	prefs->SIDFilters = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "sid_filters")));

	prefs->Joystick1Port = gtk_combo_box_get_active(GTK_COMBO_BOX(gtk_builder_get_object(builder, "joystick1_port")));
	prefs->Joystick2Port = gtk_combo_box_get_active(GTK_COMBO_BOX(gtk_builder_get_object(builder, "joystick2_port")));
	prefs->JoystickSwap = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "joystick_swap")));

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
	ghost_widget("drive9_eject", prefs->Emul1541Proc);
	ghost_widget("drive10_eject", prefs->Emul1541Proc);
	ghost_widget("drive11_eject", prefs->Emul1541Proc);
	ghost_widget("drive9_next_disk", prefs->Emul1541Proc);
	ghost_widget("drive10_next_disk", prefs->Emul1541Proc);
	ghost_widget("drive11_next_disk", prefs->Emul1541Proc);
	ghost_widget("map_slash", prefs->Emul1541Proc);

	ghost_widget("scaling_numerator", prefs->DisplayType == DISPTYPE_SCREEN);

	ghost_widget("sid_filters", prefs->SIDType != SIDTYPE_DIGITAL_6581 && prefs->SIDType != SIDTYPE_DIGITAL_8580);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "sid_filters")),
	                             (prefs->SIDType == SIDTYPE_DIGITAL_6581 || prefs->SIDType == SIDTYPE_DIGITAL_8580) ? prefs->SIDFilters : (prefs->SIDType == SIDTYPE_SIDCARD ? true : false));
}


/*
 *  SAM window handling
 *  (inspired by https://github.com/SvenFestersen/GtkPyInterpreter)
 */

static void write_sam_output(std::string s, bool error)
{
	// Append non-editable text
	GtkTextIter end;
	gtk_text_buffer_get_end_iter(sam_buffer, &end);
	if (error) {
		gtk_text_buffer_insert_with_tags_by_name(sam_buffer, &end, s.c_str(), -1, "protected", "error", nullptr);
	} else {
		gtk_text_buffer_insert_with_tags_by_name(sam_buffer, &end, s.c_str(), -1, "protected", nullptr);
	}

	// Make cursor visible
	gtk_text_view_scroll_mark_onscreen(sam_view, gtk_text_buffer_get_insert(sam_buffer));

	// Move "start of input" mark to end of text
	gtk_text_buffer_get_end_iter(sam_buffer, &end);
	gtk_text_buffer_move_mark(sam_buffer, sam_input_start, &end);
}

extern "C" gboolean on_sam_view_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	const char * key = gdk_keyval_name(event->keyval);
	if (strcmp(key, "Return") == 0) {

		// Return pressed, fetch text input since last mark set
		GtkTextIter start, end;
		gtk_text_buffer_get_iter_at_mark(sam_buffer, &start, sam_input_start);
		gtk_text_buffer_get_end_iter(sam_buffer, &end);
		const char * input = gtk_text_buffer_get_text(sam_buffer, &start, &end, true);

		// Append newline and make input non-editable
		gtk_text_buffer_insert(sam_buffer, &end, "\n", -1);
		gtk_text_buffer_get_iter_at_mark(sam_buffer, &start, sam_input_start);	// 'insert' has invalidated the iterators
		gtk_text_buffer_get_end_iter(sam_buffer, &end);
		gtk_text_buffer_apply_tag_by_name(sam_buffer, "protected", &start, &end);

		// Execute command
		std::string cmdOutput, cmdError;
		SAM_Exec(input, cmdOutput, cmdError);

		// Show output and prompt
		write_sam_output(cmdOutput.c_str());
		if (! cmdError.empty()) {
			write_sam_output(cmdError.c_str(), true);
		}
		write_sam_output(SAM_GetPrompt());

		return true;

	} else if (strcmp(key, "Up") == 0) {

		// Suppress "up" key
		// TODO: command history?
		return true;

	} else if (strcmp(key, "Left") == 0) {

		// Prevent cursor from moving into prompt
		GtkTextIter cursor, input_start;
		gtk_text_buffer_get_iter_at_mark(sam_buffer, &cursor, gtk_text_buffer_get_insert(sam_buffer));
		gtk_text_buffer_get_iter_at_mark(sam_buffer, &input_start, sam_input_start);
		return gtk_text_iter_equal(&cursor, &input_start);

	} else if (strcmp(key, "Home") == 0) {

		// Move cursor to start of input
		GtkTextIter input_start;
		gtk_text_buffer_get_iter_at_mark(sam_buffer, &input_start, sam_input_start);
		gtk_text_buffer_place_cursor(sam_buffer, &input_start);
		return true;
	}

	return false;
}

extern "C" void on_sam_copy_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	gtk_text_buffer_copy_clipboard(sam_buffer, gtk_clipboard_get_default(gdk_display_get_default()));
}

extern "C" void on_sam_select_all_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkTextIter start, end;
	gtk_text_buffer_get_start_iter(sam_buffer, &start);
	gtk_text_buffer_get_end_iter(sam_buffer, &end);
	gtk_text_buffer_select_range(sam_buffer, &start, &end);
}

extern "C" void on_sam_clear_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	gtk_text_buffer_set_text(sam_buffer, "", -1);
	write_sam_output(SAM_GetPrompt());
}

extern "C" void on_sam_close_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(builder, "sam_win")));
}


/*
 *  Signal handlers
 */

extern "C" void on_ok_clicked(GtkButton *button, gpointer user_data)
{
	gtk_widget_hide(GTK_WIDGET(prefs_win));
	gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(builder, "about_win")));
	gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(builder, "sam_win")));

	result = true;
	gtk_main_quit();
}

extern "C" void on_quit_clicked(GtkButton *button, gpointer user_data)
{
	result = false;
	gtk_main_quit();
}

extern "C" void on_load_snapshot(GtkMenuItem *menuitem, gpointer user_data)
{
	gtk_window_set_title(GTK_WINDOW(snapshot_dialog), "Load Snapshot");
	gtk_file_chooser_set_action(GTK_FILE_CHOOSER(snapshot_dialog), GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_button_set_label(GTK_BUTTON(snapshot_accept_button), "Load");

	bool load_ok = false;
	std::string message;

	char * filename = nullptr;

	gint res = gtk_dialog_run(GTK_DIALOG(snapshot_dialog));
	if (res == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(snapshot_dialog));
		load_ok = TheC64->LoadSnapshot(filename, prefs, message);
		if (load_ok) {
			message = std::format("Snapshot file '{}' loaded.", filename);
		}
		SAM_GetState(TheC64);	// Get new state from snapshot
		set_values();			// Drive settings may have changed
	}

	gtk_widget_hide(snapshot_dialog);

	if (res == GTK_RESPONSE_ACCEPT) {
		GtkWidget * msg = gtk_message_dialog_new(prefs_win, GTK_DIALOG_MODAL,
			load_ok ? GTK_MESSAGE_INFO : GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
			message.c_str()
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
	gtk_window_set_title(GTK_WINDOW(snapshot_dialog), "Save Snapshot");
	gtk_file_chooser_set_action(GTK_FILE_CHOOSER(snapshot_dialog), GTK_FILE_CHOOSER_ACTION_SAVE);
	gtk_button_set_label(GTK_BUTTON(snapshot_accept_button), "Save");

	bool save_ok = false;
	std::string message;

	char * filename = nullptr;

	gint res = gtk_dialog_run(GTK_DIALOG(snapshot_dialog));
	if (res == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(snapshot_dialog));
		save_ok = TheC64->SaveSnapshot(filename, message);
		if (save_ok) {
			message = std::format("Snapshot file '{}' saved.", filename);
		}
		SAM_GetState(TheC64);	// Saving a snapshot may advance the state a few cycles in Frodo SC
	}

	gtk_widget_hide(snapshot_dialog);

	if (res == GTK_RESPONSE_ACCEPT) {
		GtkWidget * msg = gtk_message_dialog_new(prefs_win, GTK_DIALOG_MODAL,
			save_ok ? GTK_MESSAGE_INFO : GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
			message.c_str()
		);
		gtk_dialog_run(GTK_DIALOG(msg));
		gtk_widget_destroy(msg);
	}

	if (filename) {
		g_free(filename);
	}
}

extern "C" void on_create_image(GtkMenuItem *menuitem, gpointer user_data)
{
	bool save_ok = false;
	char * filename = nullptr;

	gint res = gtk_dialog_run(GTK_DIALOG(create_image_dialog));
	if (res == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(create_image_dialog));
		save_ok = CreateImageFile(filename);
	}

	gtk_widget_hide(create_image_dialog);

	GtkWidget * msg = nullptr;
	if (save_ok) {
		msg = gtk_message_dialog_new(prefs_win,
			GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
			"Disk image file '%s' created.", filename
		);
	} else if (res == GTK_RESPONSE_ACCEPT) {
		msg = gtk_message_dialog_new(prefs_win,
			GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
			"Can't create disk image file '%s'.", filename
		);
	}
	if (msg) {
		gtk_dialog_run(GTK_DIALOG(msg));
		gtk_widget_destroy(msg);
	}

	if (filename) {
		g_free(filename);
	}
}

extern "C" void on_shortcuts_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkBuilder * shortcuts_builder = gtk_builder_new_from_string(shortcuts_win_ui, -1);
	GtkWindow * shortcuts_win = GTK_WINDOW(gtk_builder_get_object(shortcuts_builder, "shortcuts_win"));
	gtk_window_set_transient_for(shortcuts_win, prefs_win);
	gtk_window_present(shortcuts_win);
	g_object_unref(shortcuts_builder);
}

extern "C" void on_about_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkAboutDialog * about_win = GTK_ABOUT_DIALOG(gtk_builder_get_object(builder, "about_win"));
	gtk_about_dialog_set_program_name(about_win, VERSION_STRING);
	gtk_window_present(GTK_WINDOW(about_win));
}

extern "C" void on_show_sam_monitor(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWindow * sam_win = GTK_WINDOW(gtk_builder_get_object(builder, "sam_win"));
	gtk_window_present(sam_win);

	// Place cursor at end
	GtkTextIter end;
	gtk_text_buffer_get_end_iter(sam_buffer, &end);
	gtk_text_buffer_place_cursor(sam_buffer, &end);
	gtk_text_view_scroll_mark_onscreen(sam_view, gtk_text_buffer_get_insert(sam_buffer));
}

extern "C" void on_emul1541_proc_toggled(GtkToggleButton *button, gpointer user_data)
{
	prefs->Emul1541Proc = gtk_toggle_button_get_active(button);
	ghost_widgets();
}

extern "C" void on_drive_path_file_set(GtkFileChooserButton *button, gpointer user_data)
{
	gchar *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(button));
	int type;
	if (! IsMountableFile(path, type)) {

		// Not a disk image file, mount the parent directory instead
		gchar *dir = g_path_get_dirname(path);
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(button), dir);
		g_free(dir);
	}
	g_free(path);
}

extern "C" void on_drive8_eject_clicked(GtkButton *button, gpointer user_data)
{
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(gtk_builder_get_object(builder, "drive8_path")), "");
}

extern "C" void on_drive9_eject_clicked(GtkButton *button, gpointer user_data)
{
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(gtk_builder_get_object(builder, "drive9_path")), "");
}

extern "C" void on_drive10_eject_clicked(GtkButton *button, gpointer user_data)
{
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(gtk_builder_get_object(builder, "drive10_path")), "");
}

extern "C" void on_drive11_eject_clicked(GtkButton *button, gpointer user_data)
{
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(gtk_builder_get_object(builder, "drive11_path")), "");
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

extern "C" gboolean on_sam_win_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	// Prevent destruction
	return gtk_widget_hide_on_delete(widget);
}
