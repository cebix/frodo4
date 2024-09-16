/*
 *  SID_SDL.h - 6581 emulation, SDL specific stuff
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

#include <SDL_audio.h>


/*
 *  Initialization
 */

void DigitalRenderer::init_sound()
{
	SDL_AudioSpec desired;
	SDL_zero(desired);
	SDL_zero(obtained);

	// Set up desired output format
	desired.freq = SAMPLE_FREQ;
	desired.format = AUDIO_S16SYS;
	desired.channels = 1;
	desired.samples = 512;
	desired.callback = buffer_proc;
	desired.userdata = this;

	// Open output device
	device_id = SDL_OpenAudioDevice(NULL, false, &desired, &obtained, SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
	if (device_id == 0) {
		fprintf(stderr, "WARNING: Cannot open audio: %s\n", SDL_GetError());
		return;
	}

	// Start sound output
	Resume();

	ready = true;
}


/*
 *  Destructor
 */

DigitalRenderer::~DigitalRenderer()
{
	if (device_id) {
		SDL_CloseAudioDevice(device_id);
	}
}


/*
 *  Pause sound output
 */

void DigitalRenderer::Pause()
{
	if (device_id) {
		SDL_PauseAudioDevice(device_id, true);
	}
}


/*
 *  Resume sound output
 */

void DigitalRenderer::Resume()
{
	if (device_id) {
		SDL_PauseAudioDevice(device_id, false);
	}
}


/*
 *  Callback function 
 */

void DigitalRenderer::buffer_proc(void * userdata, uint8_t * buffer, int size)
{
	DigitalRenderer * renderer = (DigitalRenderer *) userdata;
	renderer->calc_buffer((int16_t *) buffer, size);
}
