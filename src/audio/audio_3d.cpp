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

#include "audio_3d.h"
#include "audio_controller.h"

audio_3d::audio_3d(const audio_store::audio_data& data, const string& identifier_) : audio_source(data, identifier_) {
	// Try to create 2 Effects
	AL_CLEAR_ERROR();
	AL(alGenEffects((ALsizei)ui_effect.size(), &ui_effect[0]));
	if (AL_IS_ERROR()) return;

	// Try to create a Filter
	AL_CLEAR_ERROR();
	AL(alGenFilters((ALsizei)ui_filter.size(), &ui_filter[0]));
	if (AL_IS_ERROR()) a2e_error("failed to generate a filter!");

	// Check, if there is a filter
	if (alIsFilter(ui_filter[0])) {
		// Set Filter type to Low-Pass and set parameters
		AL(alFilteri(ui_filter[0], AL_FILTER_TYPE, AL_FILTER_LOWPASS));
		if (AL_IS_ERROR()) {
			a2e_error("Low Pass Filter not supported");
		} else {
			AL(alFilterf(ui_filter[0], AL_LOWPASS_GAIN, 0.5f));
			AL(alFilterf(ui_filter[0], AL_LOWPASS_GAINHF, 0.5f));
		}
	}

	//
	set_position(float3(0.0f));
	set_velocity(data.velocity);
	set_reference_distance(data.reference_distance);
	set_rolloff_factor(data.rolloff_factor);
	set_max_distance(data.max_distance);
	set_volume(data.volume);
	AL(alSourcei(source, AL_SOURCE_RELATIVE, AL_FALSE));
}

audio_3d::~audio_3d() {
	// Delete Filters, Effects and AuxiliaryEffectSlots
	AL_CLEAR_ERROR();
	AL(alDeleteFilters((ALsizei)ui_filter.size(), &ui_filter[0]));
	if (AL_IS_ERROR()) a2e_error("could not delete ui_filter");
	AL(alDeleteEffects((ALsizei)ui_effect.size(), &ui_effect[0]));
	if (AL_IS_ERROR()) a2e_error("could not delete ui_effect");
}

void audio_3d::set_effect(const AUDIO_EFFECT& effect) const {
	AL_CLEAR_ERROR();
	switch(effect) {
		case AUDIO_EFFECT::REVERB:
			if (alIsEffect(ui_effect[0])) {
				AL(alEffecti(ui_effect[0], AL_EFFECT_TYPE, AL_EFFECT_REVERB));
				if (AL_IS_ERROR()) a2e_error("Effect REVERB not supported");
				else AL(alEffectf(ui_effect[0], AL_REVERB_DECAY_TIME, 5.0f));
			}
			AL(alAuxiliaryEffectSloti(ac->get_ui_effect_slot(0), AL_EFFECTSLOT_EFFECT, ui_effect[0]));
			break;
		case AUDIO_EFFECT::ECHO:
			if (alIsEffect(ui_effect[1])) {
				AL(alEffecti(ui_effect[1], AL_EFFECT_TYPE, AL_EFFECT_ECHO));
				if (AL_IS_ERROR()) a2e_error("Effect ECHO not supported");
				else AL(alEffectf(ui_effect[1], AL_ECHO_FEEDBACK, 0.5f));
			}
			AL(alAuxiliaryEffectSloti(ac->get_ui_effect_slot(1), AL_EFFECTSLOT_EFFECT, ui_effect[1]));
			break;
		case AUDIO_EFFECT::NONE:
			break;
	}

	// Configure Source Auxiliary Effect Slot Sends
	AL(alSource3i(source, AL_AUXILIARY_SEND_FILTER, ac->get_ui_effect_slot(0), 0, ui_filter[0]));
	if (AL_IS_ERROR()) a2e_error("Failed to configure Source Send 0");
	AL(alSource3i(source, AL_AUXILIARY_SEND_FILTER, ac->get_ui_effect_slot(1), 1, ui_filter[0]));
	if (AL_IS_ERROR()) a2e_error("Failed to configure Source Send 1");

	// Filter source, a generated Source
	AL(alSourcei(source, AL_DIRECT_FILTER, ui_filter[0]));
	if (!AL_IS_ERROR()) {
		// Remove filter from source
		AL(alSourcei(source, AL_DIRECT_FILTER, AL_FILTER_NULL));
		AL_CLEAR_ERROR();
	}

	// Filter the Source send 0 form source to Auxiliary Effect Slot
	// ac->get_ui_effect_slot(0] using Filter ui_filter[0)
	AL(alSource3i(source, AL_AUXILIARY_SEND_FILTER, ac->get_ui_effect_slot(0), 0, ui_filter[0]));
	if (!AL_IS_ERROR()) {
		// Remove Filter from Source Auxiliary Send
		AL(alSource3i(source, AL_AUXILIARY_SEND_FILTER, ac->get_ui_effect_slot(0), 0, AL_FILTER_NULL));
		AL_CLEAR_ERROR();
	}
}

void audio_3d::unset_effect() const {
	AL(alSource3i(source, AL_AUXILIARY_SEND_FILTER, AL_EFFECTSLOT_NULL, 0, AL_FILTER_NULL));
}

const float3& audio_3d::get_position() const {
	return position;
}

void audio_3d::set_position(const float3& pos) {
	ac->acquire_context();
	AL(alSource3f(source, AL_POSITION, pos.x, pos.y, pos.z));
	ac->release_context();
	position = pos;
}

const float3& audio_3d::get_velocity() const {
	return velocity;
}

void audio_3d::set_velocity(const float3& vel) {
	ac->acquire_context();
	AL(alSource3f(source, AL_VELOCITY, vel.x, vel.y, vel.z));
	ac->release_context();
	velocity = vel;
}

float audio_3d::get_reference_distance() const {
	return ref_distance;
}

void audio_3d::set_reference_distance(const float ref_d) {
	ac->acquire_context();
	AL(alSourcef(source, AL_REFERENCE_DISTANCE, ref_d));
	ac->release_context();
	ref_distance = ref_d;
}

float audio_3d::get_rolloff_factor() const {
	return rolloff_factor;
}

void audio_3d::set_rolloff_factor(const float rolloff_f) {
	ac->acquire_context();
	AL(alSourcef(source, AL_ROLLOFF_FACTOR, rolloff_f));
	ac->release_context();
	rolloff_factor = rolloff_f;
}

float audio_3d::get_max_distance() const {
	return max_distance;
}

void audio_3d::set_max_distance(const float max_d) {
	ac->acquire_context();
	AL(alSourcef(source, AL_MAX_DISTANCE, max_d));
	ac->release_context();
	max_distance = max_d;
}

float audio_3d::get_volume() const {
	return volume;
}

void audio_3d::set_volume(const float& vol) {
	ac->acquire_context();
	AL(alSourcef(source, AL_GAIN, (vol * conf::get<float>("volume.sound"))));
	ac->release_context();
	volume = vol;
}

bool audio_3d::is_playing() const {
	ALint state = 0;
	ac->acquire_context();
	AL(alGetSourcei(source, AL_SOURCE_STATE, &state));
	ac->release_context();
	return (state == AL_PLAYING);
}

bool audio_3d::is_initial() const {
	ALint state = 0;
	ac->acquire_context();
	AL(alGetSourcei(source, AL_SOURCE_STATE, &state));
	ac->release_context();
	return (state == AL_INITIAL);
}
