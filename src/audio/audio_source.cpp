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

#include "audio_source.h"
#include "audio_controller.h"

audio_source::audio_source(const audio_store::audio_data& data, const string& identifier_) : identifier(identifier_) {
	AL(alGenSources(1, &source)); // generate one source
	AL(alSourcei(source, AL_BUFFER, data.buffer));
}

audio_source::~audio_source() {
	if(alIsSource(source)) alDeleteSources(1, &source);
}

void audio_source::play() {
	ac->acquire_context();
	AL(alSourcePlay(source));
	ac->release_context();
	playing = true;
}

void audio_source::stop() {
	ac->acquire_context();
	AL(alSourceStop(source));
	ac->release_context();
	playing = false;
}

void audio_source::halt() {
	ac->acquire_context();
	AL(alSourcePause(source));
	ac->release_context();
	playing = true;
}

void audio_source::loop(const bool state) {
	ac->acquire_context();
	AL(alSourcei(source, AL_LOOPING, (state ? AL_TRUE : AL_FALSE)));
	ac->release_context();
	looping = (state ? true : false);
}

bool audio_source::set_identifier(const string& identifier_) {
	if(!ac->move_audio_source(identifier, identifier_)) {
		return false;
	}
	identifier = identifier_;
	return true;
}

const string& audio_source::get_identifier() const {
	return identifier;
}

bool audio_source::is_playing() const {
	return playing;
}

bool audio_source::is_looping() const {
	return looping;
}

void audio_source::set_play_on_load(const bool play_on_load_) {
	play_on_load = play_on_load_;
}

bool audio_source::get_play_on_load() const {
	return play_on_load;
}

void audio_source::set_can_be_killed(const bool can_be_killed_) {
	can_be_killed = can_be_killed_;
}

bool audio_source::get_can_be_killed() const {
	return can_be_killed;
}
