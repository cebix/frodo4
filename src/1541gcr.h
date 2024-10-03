/*
 *  1541gcr.h - Emulation of 1541 GCR disk reading/writing
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

#ifndef C1541GCR_H
#define C1541GCR_H

#include <string>

#include "1541d64.h"


// Number of supported half-tracks
constexpr unsigned NUM_HALFTRACKS = 84;


class MOS6502_1541;
class Prefs;
struct GCRDiskState;


// 1541 GCR-level disk emulation
class GCRDisk {
public:
	GCRDisk(uint8_t *ram1541);
	~GCRDisk();

	void SetCPU(MOS6502_1541 * cpu) { the_cpu = cpu; }

	void Reset();

	void GetState(GCRDiskState * s) const;
	void SetState(const GCRDiskState * s);
	void NewPrefs(const Prefs * prefs);

	void SetMotor(bool on) { motor_on = on; }
	void MoveHeadOut();
	void MoveHeadIn();

	bool SyncFound(uint32_t cycle_counter);
	bool ByteReady(uint32_t cycle_counter);
	uint8_t ReadGCRByte(uint32_t cycle_counter);
	bool WPSensorClosed(uint32_t cycle_counter);

	void WriteSector();
	void FormatTrack();

private:
	void open_d64_file(const std::string & filepath);
	void close_d64_file();

	int read_sector(unsigned track, unsigned sector, uint8_t *buffer);
	bool write_sector(unsigned track, unsigned sector, const uint8_t *buffer);
	void format_disk();

	unsigned secnum_from_ts(unsigned track, unsigned sector);
	int offset_from_ts(unsigned track, unsigned sector);

	void gcr_conv4(const uint8_t * from, uint8_t * to);
	void sector2gcr(unsigned track, unsigned sector, uint8_t * gcr);
	void disk2gcr();

	void advance_disk_change_seq(uint32_t cycle_counter);
	void rotate_disk(uint32_t cycle_counter);

	uint8_t * ram;				// Pointer to 1541 RAM
	MOS6502_1541 * the_cpu;		// Pointer to 1541 CPU object
	FILE * the_file;			// File pointer for .d64 file
	unsigned image_header;		// Length of .d64/.x64 file header

	uint8_t disk_id1, disk_id2;			// ID of disk
	uint8_t error_info[NUM_SECTORS_40];	// Sector error information (1 byte/sector)

	uint8_t * gcr_data[NUM_HALFTRACKS];			// GCR data for each half-track (nullptr = not present)
	size_t gcr_track_length[NUM_HALFTRACKS];	// Number of GCR bytes for each half-track (nullptr = not present)

	unsigned current_halftrack;		// Current halftrack number (0..NUM_HALFTRACKS-1)
	size_t gcr_offset;				// Offset of GCR data byte under R/W head, relative to gcr_data[current_halftrack]
									// Note: This is never 0, so we can access the previous GCR byte for sync detection

	uint32_t disk_change_cycle;	// Cycle of last disk change sequence step
	unsigned disk_change_seq;	// Disk change WP sensor sequence step (counts down to 0)

	uint32_t last_byte_cycle;	// Cycle when last byte was available
	uint8_t byte_latch;			// Latch for read GCR byte

	bool motor_on;				// Flag: Spindle motor on
	bool write_protected;		// Flag: Disk write-protected
	bool on_sync;				// Flag: Sync detected
	bool byte_ready;			// Flag: GCR byte ready for reading
};


// 1541 GCR state
struct GCRDiskState {
	uint32_t current_halftrack;
	uint32_t gcr_offset;

	uint32_t last_byte_cycle;
	uint32_t disk_change_cycle;

	uint8_t byte_latch;
	uint8_t disk_change_seq;

	bool motor_on;
	bool write_protected;
	bool on_sync;
	bool byte_ready;
};


#endif // ndef C1541GCR_H
