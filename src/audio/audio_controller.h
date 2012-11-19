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

#ifndef __SB_AUDIO_CONTROLLER_H__
#define __SB_AUDIO_CONTROLLER_H__

#include "sb_global.h"
#include "audio_headers.h"
#include "audio_source.h"
#include "audio_3d.h"
#include "audio_background.h"

class audio_3d;
class audio_background;

class audio_controller {
public:
	audio_controller();
	~audio_controller();
	
	void run();
	
	void acquire_context();
	bool try_acquire_context();
	void release_context();
	
	audio_3d* add_audio_3d(const string& source_identifier, const string& instance_identifier, const bool destruct_when_played = false);
	audio_background* add_audio_background(const string& source_identifier, const string& instance_identifier);
	bool delete_audio_source(const string& identifier);
	audio_source* get_audio_source(const string& identifier) const;
	bool move_audio_source(const string& old_identifier, const string& new_identifier);

	void set_position(const float3& pos);
	const float3& get_position() const;
	void set_orientation(const float3& forward_vec, const float3& up_vec);
	const array<float3, 2>& get_orientation() const;

	const ALuint& get_ui_effect_slot(const size_t& index) const;

	void reset_volume();
	void kill_audio_sources();
	
	// map helpers:
	void set_active_map_background(audio_background* bg);
	audio_background* get_active_map_background() const;

protected:
	ALCdevice* device = nullptr;
	ALCcontext* context = nullptr;
	unordered_map<string, audio_source*> sources;
	bool add_audio_source(const string& identifier, audio_source* src);
	
	float3 position;
	array<float3, 2> orientation;
	
	bool open_device();
	bool close_device();
	bool init_efx();
	bool deinit_efx();
	void init_mp3();
	void deinit_mp3();
	
	array<ALuint, 2> ui_effect_slot { { 0, 0 } };
	array<ALCint, 4> attribs { { 0, 0, 0, 0 } };
	
	set<audio_3d*> auto_destruct_sources;
	recursive_mutex auto_destruct_lock;
	
	recursive_mutex ctx_lock;
	atomic<unsigned int> ctx_active_locks { 0 };
	void handle_acquire();
	
	audio_background* map_bg = nullptr;
	
};

#endif
