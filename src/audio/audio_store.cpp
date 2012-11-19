/*
 *  Blocks In Motion
 *  Copyright (C) 2012 Florian Ziesche
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

#include "audio_store.h"
#include "audio_controller.h"
#include <engine.h>
#include <threading/task.h>

constexpr float audio_store::default_volume;
const float3 audio_store::default_velocity = float3(0.0f);
constexpr float audio_store::default_reference_distance;
constexpr float audio_store::default_rolloff_factor;
constexpr float audio_store::default_max_distance;

audio_store::audio_store() {
	as = this;
	task::spawn([]() {
		// load all audio files in data/music/
		ac->acquire_context();
		as->load_file(e->data_path("music/bg_menu.mp3"), "BG_MENU"); // load menu background music first
		ac->release_context();
		SDL_Delay(100); // should be enough to give the menu a chance to play it
		this_thread::yield();
		
		ac->acquire_context(); // acquire context so nothing tries to use an unloaded sound yet
		const auto files(core::get_file_list(e->data_path("music/"), "mp3"));
		for(const auto& file : files) {
			// don't load background audio
			if(file.first.substr(0, 3) == "bg_") continue;
			
			const string identifier(core::str_to_upper(file.first.substr(0, file.first.rfind("."))));
			if(as->load_file(e->data_path("music/"+file.first), identifier,
							 vector<AUDIO_EFFECT> { AUDIO_EFFECT::REVERB, AUDIO_EFFECT::ECHO }) == nullptr) {
				a2e_error("couldn't load file \"%s\" for identifier \"%s\"!", file.first, identifier);
				continue;
			}
		}
		ac->release_context();
	});
}

audio_store::~audio_store() {
	for(const auto& data : store) {
		if(alIsBuffer(data.second->buffer)) alDeleteBuffers(1, &data.second->buffer);
	}
}

audio_store::audio_data* audio_store::load_file(const string& filename_, const string& identifier, const vector<AUDIO_EFFECT>& effects) {
	const string filename = (conf::get<bool>("debug.no_audio") ? e->data_path("music/none.mp3") : filename_);
	
	//
	if(mpg123_open(mh, filename.c_str()) != MPG123_OK) {
		a2e_error("mpg123 couldn't open file \"%s\": %s!", filename, mpg123_strerror(mh));
		return nullptr;
	}
	
	int channels = 0;
	int encoding = 0;
	long int rate = 0;
	mpg123_format_all(mh); // allow all formats again
	if(mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
		a2e_error("trouble with mpg123 in file \"%s\": %s", filename, mpg123_strerror(mh));
		mpg123_close(mh);
		return nullptr;
	}
	
	if(encoding != MPG123_ENC_SIGNED_16 && encoding != MPG123_ENC_FLOAT_32) {
		a2e_error("bad encoding in file \"%s\": %X!", filename, encoding);
		mpg123_close(mh);
		return nullptr;
	}
	
	// set the wanted (and only) format
	mpg123_format_none(mh);
	mpg123_format(mh, rate, channels, encoding);
	
	size_t buffer_size = mpg123_outblock(mh);
	unsigned char* buffer = new unsigned char[buffer_size];
	
	// decode data
	vector<unsigned char> data;
	int err = MPG123_OK;
	size_t done = 0;
	do {
		err = mpg123_read(mh, buffer, buffer_size, &done);
		data.insert(end(data), buffer, buffer + done);
	} while(err == MPG123_OK);
	
	delete [] buffer;
	
	if(err != MPG123_DONE) {
		a2e_error("decoding ended prematurely because: %s",
				  err == MPG123_ERR ? mpg123_strerror(mh) : mpg123_plain_strerror(err));
		mpg123_close(mh);
		return nullptr;
	}
	mpg123_close(mh); // cleanup for current file
	
	a2e_debug("\"%s\": rate %s, channels %s, encoding %s", identifier, rate, channels, encoding);
	
	//
	ac->acquire_context();
	AL_CLEAR_ERROR(); // clear error code
	const ALenum format = (channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16);
	ALuint buffer_data = 0;
	AL(alGenBuffers(1, &buffer_data));
	AL(alBufferData(buffer_data, format, &data[0], (ALsizei)data.size(), (ALsizei)rate));
	
	audio_data* adata = new audio_data {
		filename,
		buffer_data,
		format,
		(ALsizei)rate,
		default_velocity,
		default_volume,
		default_reference_distance,
		default_rolloff_factor,
		default_max_distance,
		effects
	};
	store.insert(make_pair(identifier, adata));
	ac->release_context();
	
	eevt->add_event(EVENT_TYPE::AUDIO_STORE_LOAD, make_shared<audio_store_load_event>(SDL_GetTicks(), identifier));
	return adata;
}

bool audio_store::has_audio_data(const string& identifier) const {
	return (store.count(identifier) > 0);
}

const audio_store::audio_data& audio_store::get_audio_data(const string& identifier) const {
	return *store.at(identifier);
}

const unordered_map<string, audio_store::audio_data*>& audio_store::get_store() const {
	return store;
}
