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

#ifndef __SB_PHYSICS_PLAYER_H__
#define __SB_PHYSICS_PLAYER_H__

#include "sb_global.h"
#include "physics_entity.h"
#include <atomic>

class physics_player : public physics_entity {
public:
	physics_player(const float3& position);
	virtual ~physics_player();
	
	virtual void physics_update();
	virtual void graphics_update();

	void set_health(const float& value);
	float get_health() const;
	
	void set_event_input(const bool& state);
	bool get_event_input() const;
	void set_camera_control(const bool& state);
	bool get_camera_control() const;
	void set_pulled(const bool& state);
	bool get_pulled() const;

	virtual void reset_parameters();
	void set_timer(int value);
	
protected:
	// [right, left, up, down]
	array<atomic<bool>, 4> move_state;
	atomic<bool> jump_state { false };
	atomic<bool> jumping { false };
	event::handler evt_handler;
	bool event_handler(EVENT_TYPE type, shared_ptr<event_object> obj);
	
	bool event_input = true;
	bool camera_control = true;
	
	virtual void block_step(const uint3& block) const;
	virtual void step(const float3& pos) const;

	bool is_pulled = false;

	float health = 100.0f;

	float3 target_position;
	float3 prev_pos;

	int timer_mseconds = 0;
};

#endif
