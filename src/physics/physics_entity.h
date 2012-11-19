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

#ifndef __SB_PHYSICS_ENTITY_H__
#define __SB_PHYSICS_ENTITY_H__

#include "sb_global.h"
#include "rigid_body.h"

class a2estatic;
class a2ematerial;
class physics_entity {
public:
	physics_entity(const float3& position, const float2 character_size = float2(0.5f, 1.25f), const string model_filename = "", const string material_filename = "");
	virtual ~physics_entity();
	
	// note: physics_update is called from the physics controller thread
	virtual void physics_update();
	// note: graphics_update is called from the main render thread/loop
	virtual void graphics_update();
	
	virtual void set_position(const float3& position);
	virtual const float3& get_position() const;
	
	virtual void set_rotation(const float3& rotation);
	
	virtual void set_move_direction(const float3& direction);
	virtual const float3& get_move_direction() const;
	
	virtual void set_speed(const float& speed);
	virtual const float& get_speed() const;
	virtual void reset_speed();
	
	virtual void set_jump_strength(const float& strength);
	virtual const float& get_jump_strength() const;
	virtual void reset_jump_strength();

	rigid_info* get_character_info();
	rigid_body* get_character_body();

protected:
	rigid_info* character_rinfo = nullptr;
	rigid_body* character_body = nullptr;
	btRigidBody* body = nullptr;
	
	a2estatic* mdl = nullptr;
	a2ematerial* mat = nullptr;
	
	float3 move_direction;
	
	float3 cur_velocity; // consider this read-only
	
	//
	const float2 character_size; // (radius, half-height)
	float step_height = 1.0f; // one block
	float speed = 5.0f;
	float default_speed = speed;
	float jump_strength = 5.0f;
	float default_jump_strength = jump_strength;
	float prev_velocity_scale = 0.85f;
	
	// entity step vars/functions
	float3 prev_pos;
	uint3 prev_block;
	float cur_step_length = 0.0f;
	virtual void block_step(const uint3& block) const;
	virtual void step(const float3& pos) const;

};

#endif
