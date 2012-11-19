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

#ifndef __SB_EVENTS_H__
#define __SB_EVENTS_H__

// add all additional event types that should be handled/accepted by the engine event handler here
// note: yes, there is slight macro voodoo involved here, but w/o enum inheritance there aren't many options
#define A2E_USER_EVENT_TYPES \
	BLOCK_CHANGE,			/* triggered by sb_map::update when a block is changed */ \
	MAP_LOAD,				/* triggered after a map has successfully been loaded by map_storage */ \
	MAP_UNLOAD,				/* triggered right before a map is deleted */ \
	PLAYER_STEP,			/* triggered after the player moved by "one step unit" */ \
	PLAYER_BLOCK_STEP,		/* triggered after the player moved onto a new block (continuity not guaranteed!) */ \
	AI_STEP,				/* triggered after the ai moved by "one step unit" */ \
	AI_BLOCK_STEP,			/* triggered after the ai moved onto a new block (continuity not guaranteed!) */ \
	AUDIO_STORE_LOAD,		/* triggered after the audio store loaded a file successfully */

#include <gui/event.h>

// block events
template<EVENT_TYPE event_type> struct block_event_base : public event_object_base<event_type> {
	const unsigned int chunk_idx;
	const unsigned int block_idx;
	block_event_base(const unsigned int& time_, const unsigned int& chunk_idx_, const unsigned int& block_idx_)
	: event_object_base<event_type>(time_), chunk_idx(chunk_idx_), block_idx(block_idx_) {}
};

enum class BLOCK_MATERIAL : unsigned int;
template<EVENT_TYPE event_type> struct block_change_event_base : public block_event_base<event_type> {
	const BLOCK_MATERIAL old_material;
	const BLOCK_MATERIAL new_material;
	block_change_event_base(const unsigned int& time_, const unsigned int& chunk_idx_, const unsigned int& block_idx_,
							const BLOCK_MATERIAL& old_material_, const BLOCK_MATERIAL& new_material_)
	: block_event_base<event_type>(time_, chunk_idx_, block_idx_),
	old_material(old_material_), new_material(new_material_) {}
};
typedef block_change_event_base<EVENT_TYPE::BLOCK_CHANGE> block_change_event;

// map events
struct map_load_event : public event_object_base<EVENT_TYPE::MAP_LOAD> {
	const string filename;
	map_load_event(const unsigned int& time_, const string& filename_)
	: event_object_base<EVENT_TYPE::MAP_LOAD>(time_), filename(filename_) {}
};
typedef event_object_base<EVENT_TYPE::MAP_UNLOAD> map_unload_event;

// player events
struct player_step_event : public event_object_base<EVENT_TYPE::PLAYER_STEP> {
	const float3 position;
	player_step_event(const unsigned int& time_, const float3& position_)
	: event_object_base<EVENT_TYPE::PLAYER_STEP>(time_), position(position_) {}
};
struct player_block_step_event : public event_object_base<EVENT_TYPE::PLAYER_BLOCK_STEP> {
	const uint3 block;
	player_block_step_event(const unsigned int& time_, const uint3& block_)
	: event_object_base<EVENT_TYPE::PLAYER_BLOCK_STEP>(time_), block(block_) {}
};

// ai events
struct ai_step_event : public event_object_base<EVENT_TYPE::AI_STEP> {
	const float3 position;
	ai_step_event(const unsigned int& time_, const float3& position_)
	: event_object_base<EVENT_TYPE::AI_STEP>(time_), position(position_) {}
};
class ai_entity;
struct ai_block_step_event : public event_object_base<EVENT_TYPE::AI_BLOCK_STEP> {
	const uint3 block;
	const ai_entity* ai;
	ai_block_step_event(const unsigned int& time_, const uint3& block_, const ai_entity* ai_)
	: event_object_base<EVENT_TYPE::AI_BLOCK_STEP>(time_), block(block_), ai(ai_) {}
};

// audio store events
struct audio_store_load_event : public event_object_base<EVENT_TYPE::AUDIO_STORE_LOAD> {
	const string identifier;
	audio_store_load_event(const unsigned int& time_, const string& identifier_)
	: event_object_base<EVENT_TYPE::AUDIO_STORE_LOAD>(time_), identifier(identifier_) {}
};

#endif
