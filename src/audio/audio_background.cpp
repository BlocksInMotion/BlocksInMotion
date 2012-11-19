/*
 *  Blocks In Motion
 *  Copyright (C) 2012 Yannic Haupenthal
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "audio_background.h"
#include "audio_controller.h"

audio_background::audio_background(const audio_store::audio_data& data, const string& identifier_) : audio_source(data, identifier_) {
	AL(alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f));
	AL(alSource3f(source, AL_VELOCITY, 0.0f, 0.0f, 0.0f));
	AL(alSourcei(source, AL_LOOPING, AL_TRUE));
	AL(alSourcei(source, AL_ROLLOFF_FACTOR, 0.0f));
	AL(alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE));
	set_volume(data.volume);
}

audio_background::~audio_background() {
}

float audio_background::get_volume() const {
	return volume;
}

void audio_background::set_volume(const float& vol) {
	ac->acquire_context();
	AL(alSourcef(source, AL_GAIN, (volume * conf::get<float>("volume.music"))));
	ac->release_context();
	volume = vol;
}
