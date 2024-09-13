/*
 *  1541job.h - Emulation of 1541 GCR disk reading/writing
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

#ifndef _1541JOB_H
#define _1541JOB_H

#include <string>


class MOS6502_1541;
class Prefs;
struct Job1541State;

class Job1541 {
public:
	Job1541(uint8_t *ram1541);
	~Job1541();

	void GetState(Job1541State *state) const;
	void SetState(const Job1541State *state);
	void NewPrefs(const Prefs *prefs);
	void MoveHeadOut();
	void MoveHeadIn();
	bool SyncFound();
	uint8_t ReadGCRByte();
	uint8_t WPState();
	void WriteSector();
	void FormatTrack();

private:
	void open_d64_file(const std::string & filepath);
	void close_d64_file();
	bool read_sector(int track, int sector, uint8_t *buffer);
	bool write_sector(int track, int sector, uint8_t *buffer);
	void format_disk();
	int secnum_from_ts(int track, int sector);
	int offset_from_ts(int track, int sector);
	void gcr_conv4(const uint8_t *from, uint8_t *to);
	void sector2gcr(int track, int sector);
	void disk2gcr();

	uint8_t *ram;				// Pointer to 1541 RAM
	FILE *the_file;				// File pointer for .d64 file
	int image_header;			// Length of .d64/.x64 file header

	uint8_t id1, id2;			// ID of disk
	uint8_t error_info[683];	// Sector error information (1 byte/sector)

	uint8_t *gcr_data;			// Pointer to GCR encoded disk data
	uint8_t *gcr_ptr;			// Pointer to GCR data under R/W head
	uint8_t *gcr_track_start;	// Pointer to start of GCR data of current track
	uint8_t *gcr_track_end;		// Pointer to end of GCR data of current track
	unsigned current_halftrack;	// Current halftrack number (2..70)

	bool write_protected;		// Flag: Disk write-protected
	bool disk_changed;			// Flag: Disk changed (WP sensor strobe control)
};

// 1541 GCR state
struct Job1541State {
	uint32_t gcr_ptr;
	uint16_t current_halftrack;
	bool write_protected;
	bool disk_changed;
};


/*
 *  Check if R/W head is over SYNC
 */

inline bool Job1541::SyncFound()
{
	if (*gcr_ptr == 0xff) {
		return true;
	} else {
		gcr_ptr++;		// Rotate disk
		if (gcr_ptr == gcr_track_end) {
			gcr_ptr = gcr_track_start;
		}
		return false;
	}
}


/*
 *  Read one GCR byte from disk
 */

inline uint8_t Job1541::ReadGCRByte()
{
	uint8_t byte = *gcr_ptr++;	// Rotate disk
	if (gcr_ptr == gcr_track_end) {
		gcr_ptr = gcr_track_start;
	}
	return byte;
}


/*
 *  Return state of write protect sensor
 */

inline uint8_t Job1541::WPState()
{
	if (disk_changed) {	// Disk change -> WP sensor strobe
		disk_changed = false;
		return write_protected ? 0x10 : 0;
	} else {
		return write_protected ? 0 : 0x10;
	}
}

#endif
