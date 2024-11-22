/*
 *  Tape.h - Emulation of Datasette tape drive
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

#ifndef TAPE_H
#define TAPE_H

#include <string>


class MOS6526;
class Prefs;
struct TapeState;


// Datasette emulation
class Tape {
public:
	Tape(MOS6526 * cia);
	~Tape();

	void Reset();

	void GetState(TapeState * s) const;
	void SetState(const TapeState * s);

	void NewPrefs(const Prefs * prefs);

	void EmulateCycle();

	void SetMotor(bool on);
	void PressPlayButton(bool on);
	void Rewind();

	bool PlayPressed() const { return play_pressed; }
	bool TapePlaying() const { return tape_playing; }
	int TapePosition() const;

private:
	void open_image_file(const std::string & filepath);
	void close_image_file();

	void schedule_pulse();
	void trigger_pulse();

	MOS6526 * the_cia;		// Pointer to CIA object

	FILE * the_file;		// File pointer for image file
	unsigned tap_version;	// TAP file version
	uint32_t header_size;	// TAP header size
	uint32_t data_size;		// TAP data size

	uint32_t current_pos;	// Current position in image file

	bool motor_on;			// Flag: Tape motor on
	bool play_pressed;		// Flag: Play button pressed
	bool tape_playing;		// Flag: Tape playing

	int pulse_length;		// Remaining length of next pulse in cycles (-1 = no pulse pending)
};


// Datasette state
struct TapeState {
	uint32_t current_pos;
	int32_t pulse_length;
	bool play_pressed;
	// Motor state comes from the CPU
};


/*
 *  Check for tape pulse
 */

inline void Tape::EmulateCycle()
{
	if (pulse_length < 0)
		return;

	--pulse_length;
	if (pulse_length == 0) {
		trigger_pulse();
	}
}


/*
 *  Functions
 */

// Check whether file with given header (64 bytes) and size looks like a
// tape image file
extern bool IsTapeImageFile(const std::string & path, const uint8_t * header, long size);


#endif // ndef TAPE_H
