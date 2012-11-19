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

#include "audio_controller.h"
#include "audio_store.h"
#include <engine.h>

audio_controller::audio_controller() {
	open_device();
	init_efx();
	init_mp3();
	ac = this;
	as = new audio_store();
}

audio_controller::~audio_controller() {
	acquire_context();
	delete as;
	for(const auto& src : sources) {
		delete src.second;
	}
	deinit_mp3();
	deinit_efx();
	if (!close_device()) a2e_error("could not close device");
	release_context();
}

bool audio_controller::open_device() {
	// Open default OpenAL device
	device = alcOpenDevice(nullptr); // see Programmers Guide for more devices
	if (device == nullptr) {
		a2e_error("could not open device \"%s\"", device);
		return false;
	}
	return true;
}

bool audio_controller::close_device() {
	alcCloseDevice(device);
	device = nullptr;
	return true;
}

bool audio_controller::init_efx() {
	// Query for Effect Extension
	if (alcIsExtensionPresent(device, "ALC_EXT_EFX") == AL_FALSE) {
		a2e_error("EFX Extension not found!");
		return false;
	}

	// Use Context creation hint to request 2 Auxili2ry Sends per source
	attribs[0] = ALC_MAX_AUXILIARY_SENDS;
	attribs[1] = 2;

	context = alcCreateContext(device, &attribs[0]);

	// Activate the context
	alcMakeContextCurrent(context);
	
	// get efx funcs
	if(!init_efx_funcs()) {
		a2e_error("couldn't initialize efx functions!");
		return false;
	}

	// Retrieve the actual number of Aux Sends available on each Source
	AL_CLEAR_ERROR();
	ALCint i_sends = 0;
	AL(alcGetIntegerv(device, ALC_MAX_AUXILIARY_SENDS, 1, &i_sends));

	// EFX available and ready to be used, go ahead ;)
	// Try to create 2 Auxiliary Effect Slots
	AL_CLEAR_ERROR();
	AL(alGenAuxiliaryEffectSlots((ALsizei)ui_effect_slot.size(), &ui_effect_slot[0]));
	if (AL_IS_ERROR()) return false;

	// Set distance model (see Programmers Guide -> alDistanceModel)
	AL(alDistanceModel(AL_EXPONENT_DISTANCE_CLAMPED));
	return true;
}

bool audio_controller::deinit_efx() {
	AL_CLEAR_ERROR();
	AL(alDeleteAuxiliaryEffectSlots((ALsizei)ui_effect_slot.size(), &ui_effect_slot[0]));
	if (AL_IS_ERROR()) a2e_error("could not delete ui_effect_slot");
	
	alcDestroyContext(context);
	context = nullptr;
	return true;
}

const ALuint& audio_controller::get_ui_effect_slot(const size_t& index) const {
	return ui_effect_slot[index];
}

void audio_controller::init_mp3() {
	int err = MPG123_OK;
	err = mpg123_init();
	if (mpg123_init() != MPG123_OK || (mh = mpg123_new(nullptr, &err)) == nullptr) {
		a2e_error("failed to init mpg123: %s", err == MPG123_ERR ? mpg123_strerror(mh) : mpg123_plain_strerror(err));
	}
}

void audio_controller::deinit_mp3() {
	mpg123_delete(mh);
	mpg123_exit();
	mh = nullptr;
}

audio_3d* audio_controller::add_audio_3d(const string& source_identifier, const string& instance_identifier, const bool destruct_when_played) {
	acquire_context();
	if(!as->has_audio_data(source_identifier)) {
		a2e_error("audio store has no data for identifier \"%s\"!", source_identifier);
		release_context();
		return nullptr;
	}
	const string combined_identifier = source_identifier + "." + instance_identifier;
	audio_3d* src = new audio_3d(as->get_audio_data(source_identifier), combined_identifier);
	if(!add_audio_source(combined_identifier, src)) {
		delete src;
		release_context();
		return nullptr;
	}
	if(destruct_when_played) {
		auto_destruct_lock.lock();
		auto_destruct_sources.insert(src);
		auto_destruct_lock.unlock();
	}
	release_context();
	return src;
}

audio_background* audio_controller::add_audio_background(const string& source_identifier, const string& instance_identifier) {
	acquire_context();
	if(!as->has_audio_data(source_identifier)) {
		a2e_error("audio store has no data for identifier \"%s\"!", source_identifier);
		release_context();
		return nullptr;
	}
	const string combined_identifier = source_identifier + "." + instance_identifier;
	audio_background* src = new audio_background(as->get_audio_data(source_identifier), combined_identifier);
	if(!add_audio_source(combined_identifier, src)) {
		delete src;
		release_context();
		return nullptr;
	}
	release_context();
	return src;
}

bool audio_controller::add_audio_source(const string& identifier, audio_source* src) {
	if(sources.count(identifier) > 0) return false;
	sources.insert(make_pair(identifier, src));
	return true;
}

bool audio_controller::delete_audio_source(const string& identifier) {
	acquire_context();
	const auto iter = sources.find(identifier);
	if (iter == sources.cend()) {
		release_context();
		return false;
	}
	delete iter->second;
	sources.erase(iter);
	release_context();
	return true;
}

audio_source* audio_controller::get_audio_source(const string& identifier) const {
	const auto iter = sources.find(identifier);
	if (iter != sources.cend()) return iter->second;
	return nullptr;
}

bool audio_controller::move_audio_source(const string& old_identifier, const string& new_identifier) {
	const auto old_iter = sources.find(old_identifier);
	const auto new_iter = sources.find(new_identifier);
	if(old_iter == sources.end()) {
		return false;
	}
	if(new_iter != sources.end()) {
		return false;
	}
	audio_source* src = old_iter->second;
	sources.erase(old_iter);
	sources.insert(make_pair(new_identifier, src));
	return true;
}

void audio_controller::set_position(const float3& pos) {
	position = pos;
	AL(alListener3f(AL_POSITION, pos.x, pos.y, pos.z));
}

const float3& audio_controller::get_position() const {
	return position;
}

void audio_controller::set_orientation(const float3& forward_vec, const float3& up_vec) {
	orientation[0] = forward_vec;
	orientation[1] = up_vec;
	AL(alListenerfv(AL_ORIENTATION, (const ALfloat*)&orientation[0]));
}

const array<float3, 2>& audio_controller::get_orientation() const {
	return orientation;
}

void audio_controller::reset_volume() {
	acquire_context();
	for(const auto& src : sources) {
		src.second->set_volume(src.second->get_volume());
	}
	release_context();
}

void audio_controller::run() {
	if(!try_acquire_context()) return;
	set_position(-*e->get_position());
	const matrix4f inv_rot_mat(matrix4f(*e->get_rotation_matrix()).invert());
	set_orientation((float3(0.0f, 0.0f, -1.0f) * inv_rot_mat).normalized(),
					(float3(0.0f, 1.0f, 0.0f) * inv_rot_mat).normalized());
	
	//
	auto_destruct_lock.lock();
	for(auto iter = begin(auto_destruct_sources); iter != end(auto_destruct_sources);) {
		if(!(*iter)->is_playing() && !(*iter)->is_initial()) {
			delete_audio_source((*iter)->get_identifier());
			iter = auto_destruct_sources.erase(iter);
		}
		else iter++;
	}
	auto_destruct_lock.unlock();
	release_context();
}

void audio_controller::kill_audio_sources() {
	acquire_context();
	for(const auto& src : sources) {
		if (src.second->get_can_be_killed()) {
			src.second->stop();
		}
	}
	release_context();
}

bool audio_controller::try_acquire_context() {
	// note: the context lock is recursive, so one thread can lock it multiple times.
	if(!ctx_lock.try_lock()) return false;
	handle_acquire();
	return true;
}

void audio_controller::acquire_context() {
	// note: the context lock is recursive, so one thread can lock it multiple times.
	ctx_lock.lock();
	handle_acquire();
}

void audio_controller::handle_acquire() {
	// note: not a race, since there can only be one active al thread
	const unsigned int cur_active_locks = ctx_active_locks++;
	if(cur_active_locks == 0 &&
	   alcMakeContextCurrent(context) != 0) {
		if(AL_IS_ERROR()) a2e_error("couldn't make openal context current!");
	}
}

void audio_controller::release_context() {
	const unsigned int cur_active_locks = --ctx_active_locks;
	if(cur_active_locks == 0) {
		if(AL_IS_ERROR()) {
			a2e_error("couldn't release current openal context!");
		}
	}
	ctx_lock.unlock();
}

void audio_controller::set_active_map_background(audio_background* bg) {
	map_bg = bg;
}

audio_background* audio_controller::get_active_map_background() const {
	return map_bg;
}
