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

#ifndef __SB_AI_ENTITY_H__
#define __SB_AI_ENTITY_H__

#include "sb_global.h"
#include "physics_entity.h"
#include "sb_map.h"

enum class AI_TARGET_MODE : unsigned int {
	NONE,
	PATROL,
	FOLLOW,
	WAIT,
	ATTACK,
	WAYPOINT
};

class particle_system;
class ai_entity : public physics_entity {
public:
	ai_entity(const float3& position);
	virtual ~ai_entity();
	
	virtual void physics_update();
	virtual void graphics_update();
	
	virtual void stop_moving();
	
	virtual void turn_back();
	
	virtual float get_distance_to_player();
	
	virtual void sense();

	virtual void do_patrolling();
	virtual void force_pull_player();
	virtual void force_push_player();

	virtual void force_push_ai();

	virtual void move_to_position(const float3& pos);
	virtual void calc_move_direction(const float3& from_pos, const float3& to_pos);

	virtual bool is_patrolling() const;
	virtual bool is_attacking() const;
	virtual bool is_following() const;

	virtual void reset_parameters();
	virtual void wait(const unsigned int& seconds);

protected:
	float3 move_position;
	unsigned int timer_mseconds = 0;
	const float sensor_distance = 6.0f;
	const float attack_distance = 2.0f;
	float3 target_position;
	float3 prev_pos;

	AI_TARGET_MODE target_mode = AI_TARGET_MODE::NONE;

	const vector<sb_map::ai_waypoint*>& waypoints;
	sb_map::ai_waypoint* cur_waypoint = nullptr;
	void next_waypoint();
	void check_waypoint();

	const float weapon_strength = 40.0f;
	
	array<float2, 32> avg_rotation;
	size_t avg_rotation_index = 0;
	
	//
	particle_system* attack_ps = nullptr;
	unsigned int attack_ps_timer = 0;
	
	virtual void block_step(const uint3& block) const;
	virtual void step(const float3& pos) const;
};

#endif
