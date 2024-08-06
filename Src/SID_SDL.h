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

#include <SDL/SDL_audio.h>


/*
 *  Initialization
 */

void DigitalRenderer::init_sound(void)
{
	SDL_AudioSpec spec;
	spec.freq = SAMPLE_FREQ;
	spec.format = AUDIO_S16SYS;
	spec.channels = 1;
	spec.samples = 512;
	spec.callback = buffer_proc;
	spec.userdata = this;

	if (SDL_OpenAudio(&spec, NULL) < 0) {
		fprintf(stderr, "WARNING: Cannot open audio: %s\n", SDL_GetError());
		return;
	}

	SDL_PauseAudio(0);
	ready = true;
}


/*
 *  Destructor
 */

DigitalRenderer::~DigitalRenderer()
{
	SDL_CloseAudio();
}


/*
 *  Sample volume (for sampled voice)
 */

void DigitalRenderer::EmulateLine(void)
{
	sample_buf[sample_in_ptr] = volume;
	sample_in_ptr = (sample_in_ptr + 1) % SAMPLE_BUF_SIZE;
}


/*
 *  Pause sound output
 */

void DigitalRenderer::Pause(void)
{
	SDL_PauseAudio(1);
}


/*
 *  Resume sound output
 */

void DigitalRenderer::Resume(void)
{
	SDL_PauseAudio(0);
}


/*
 *  Callback function 
 */

void DigitalRenderer::buffer_proc(void *cookie, uint8 *buffer, int size)
{
	DigitalRenderer * renderer = (DigitalRenderer *) cookie;
	renderer->calc_buffer((int16 *) buffer, size);
}
