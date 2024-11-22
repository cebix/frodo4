/*
 *  Tape.cpp - Emulation of Datasette tape drive
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

/*
 * Incompatibilities:
 * ------------------
 *
 *  - No writing to tape implemented
 */

#include "sysdeps.h"

#include "Tape.h"
#include "CIA.h"
#include "IEC.h"
#include "Prefs.h"


/*
 *  Constructor: Open tape image file
 */

Tape::Tape(MOS6526 * cia) : the_cia(cia), the_file(nullptr)
{
	current_pos = 0;

	motor_on = false;
	play_pressed = false;
	tape_playing = false;

	open_image_file(ThePrefs.TapePath);
	Rewind();
}


/*
 *  Destructor: Close tape image file
 */

Tape::~Tape()
{
	close_image_file();
}


/*
 *  Reset tape emulation
 */

void Tape::Reset()
{
	SetMotor(false);
	PressPlayButton(false);
}


/*
 *  Preferences may have changed
 */

void Tape::NewPrefs(const Prefs * prefs)
{
	// Image file name changed?
	if (ThePrefs.TapePath != prefs->TapePath) {

		// Swap tape
		close_image_file();
		open_image_file(prefs->TapePath);
		Rewind();
	}
}


/*
 *  Change tape motor control state
 */

void Tape::SetMotor(bool on)
{
	if (motor_on != on) {
		motor_on = on;
		tape_playing = (the_file != nullptr) && motor_on && play_pressed;

		schedule_pulse();
	}
}


/*
 *  Change Play button state
 */

void Tape::PressPlayButton(bool on)
{
	if (play_pressed != on) {
		play_pressed = on;
		tape_playing = (the_file != nullptr) && motor_on && play_pressed;

		schedule_pulse();
	}
}


/*
 *  Rewind tape to start position
 */

void Tape::Rewind()
{
	if (the_file != nullptr) {
		current_pos = header_size;
		fseek(the_file, current_pos, SEEK_SET);
	}

	PressPlayButton(false);

	pulse_length = -1;
}


/*
 *  Return tape position in percent
 */

int Tape::TapePercent() const
{
	if (data_size == 0) {
		return 0;
	} else {
		return (current_pos - header_size) * 100 / data_size;
	}
}


/*
 *  Check whether file with given header (64 bytes) and size looks like a
 *  tape image file
 */

bool IsTapeImageFile(const std::string & path, const uint8_t * header, long size)
{
	return memcmp(header, "C64-TAPE-RAW", 12) == 0;
}


/*
 *  Open tape image file
 */

void Tape::open_image_file(const std::string & filepath)
{
	// Check file type
	int type;
	if (! IsMountableFile(filepath, type))
		return;
	if (type != FILE_TAPE_IMAGE)
		return;

	// Open file
	the_file = fopen(filepath.c_str(), "rb");
	if (the_file == nullptr)
		return;

	// Get version and data size
	uint8_t header[20];
	if (fread(header, 1, sizeof(header), the_file) != sizeof(header))
		goto error;

	tap_version = header[12];
	if (tap_version != 0 && tap_version != 1)
		goto error;

	header_size = 20;
	data_size = (header[19] << 24) | (header[18] << 16) | (header[17] << 8) | header[16];
	return;

error:
	fclose(the_file);
	the_file = nullptr;
}


/*
 *  Close tape image file
 */

void Tape::close_image_file()
{
	if (the_file != nullptr) {
		fclose(the_file);
		the_file = nullptr;
	}

	header_size = data_size = 0;
	current_pos = 0;
}


/*
 *  Schedule next tape pulse
 */

void Tape::schedule_pulse()
{
	// Tape ejected or stopped?
	if (the_file == nullptr || ! tape_playing) {
		pulse_length = -1;
		return;
	}

	// Pulse pending?
	if (pulse_length > 0) {
		return;
	}

	// Get next pulse from image file
	int byte = getc(the_file);
	if (byte == EOF) {
eot:	PressPlayButton(false);
		return;
	}
	++current_pos;

	if (byte) {

		// Regular short pulse
		pulse_length += byte * 8;

	} else if (tap_version == 1) {

		// Long pulse
		int lo = getc(the_file);
		if (lo == EOF)
			goto eot;
		++current_pos;

		int mid = getc(the_file);
		if (mid == EOF)
			goto eot;
		++current_pos;

		int hi = getc(the_file);
		if (hi == EOF)
			goto eot;
		++current_pos;

		pulse_length += (hi << 16) | (mid << 8) | lo;

	} else {

		// Overflow pulse
		pulse_length += 1024 * 8;
	}

	if (pulse_length < 0) {
		pulse_length = 0;
	}
}


/*
 *  Trigger CIA and get next pulse length
 */

void Tape::trigger_pulse()
{
	the_cia->TriggerFlagLine();
	schedule_pulse();
}


/*
 *  Get state
 */

void Tape::GetState(TapeState * s) const
{
	s->current_pos = current_pos;
	s->pulse_length = pulse_length;
	s->play_pressed = play_pressed;
}


/*
 *  Set state
 */

void Tape::SetState(const TapeState * s)
{
	if (the_file != nullptr) {
		current_pos = s->current_pos;
		fseek(the_file, current_pos, SEEK_SET);

		pulse_length = s->pulse_length;
		PressPlayButton(s->play_pressed);
	}
}
