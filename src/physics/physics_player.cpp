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

#include "physics_player.h"
#include "physics_controller.h"
#include "sb_map.h"
#include "game.h"
#include "save.h"
#include <engine.h>
#include <scene/scene.h>
#include <scene/camera.h>

physics_player::physics_player(const float3& position) :
physics_entity(position, float2(0.5f, 1.25f), "cylinder.a2m", "sphere.a2mtl"),
evt_handler(this, &physics_player::event_handler) {
	//
	move_state[0].store(false);
	move_state[1].store(false);
	move_state[2].store(false);
	move_state[3].store(false);
	eevt->add_event_handler(evt_handler,
							EVENT_TYPE::KEY_DOWN,
							EVENT_TYPE::KEY_UP,
							EVENT_TYPE::MOUSE_LEFT_DOWN,
							EVENT_TYPE::MOUSE_LEFT_UP,
							EVENT_TYPE::MOUSE_RIGHT_DOWN,
							EVENT_TYPE::MOUSE_RIGHT_UP,
							EVENT_TYPE::MOUSE_MIDDLE_DOWN,
							EVENT_TYPE::MOUSE_MIDDLE_UP);

	timer_mseconds = SDL_GetTicks();
	
	mdl->set_visible(false);
}

physics_player::~physics_player() {
	eevt->remove_event_handler(evt_handler);
}

void physics_player::physics_update() {
	if(health < 100.0f) {
		const auto ticks = SDL_GetTicks();
		const float temp_seconds = (float)ticks / 1000.0f;
		const float timer_seconds = (float)timer_mseconds / 1000.0f;
		const float passed_seconds = temp_seconds - timer_seconds;
		if(passed_seconds > 1.0f) {
			health += 5.0f;
			timer_mseconds = ticks;
		}
	}

	//
	if(event_input) {
		const float3 rotation(cam->get_rotation());
		float3 dir;
		float up = 0.0f;
		if(move_state[0].load()) {
			dir.x -= (float)sin((rotation.y - 90.0f) * _PIDIV180);
			dir.z += (float)cos((rotation.y - 90.0f) * _PIDIV180);
		}
		if(move_state[1].load()) {
			dir.x += (float)sin((rotation.y - 90.0f) * _PIDIV180);
			dir.z -= (float)cos((rotation.y - 90.0f) * _PIDIV180);
		}
		if(move_state[2].load()) {
			dir.x += (float)sin(rotation.y * _PIDIV180);
			dir.z -= (float)cos(rotation.y * _PIDIV180);
		}
		if(move_state[3].load()) {
			dir.x -= (float)sin(rotation.y * _PIDIV180);
			dir.z += (float)cos(rotation.y * _PIDIV180);
		}
		if(jump_state.load()) {
			if(!jumping.load()) {
				up = jump_strength / speed; // will be multiplied by speed in physics_entity::update
				jumping.store(true);
				save->inc_jump_count();
			}
			jump_state.store(false);
		}
		
		set_move_direction((dir.x + dir.z != 0.0f ? dir.normalized() : dir) + float3(0.0f, up, 0.0f));

		physics_entity::physics_update();
		
		if(fabs(cur_velocity.y) <= 0.001f && jumping.load()) {
			jumping.store(false);
		}
	}
	else {
		float3 target_dir = target_position - get_position();
		const float len = target_dir.length();
		const float l = target_position.length();
		if((l > 0.0f) && (len > 1.0f)) {
			// speed up body -> min scale 3
			target_dir = (target_dir.normalize() * std::max<float>(len, 0.5f));
			character_body->get_body()->setLinearVelocity(btVector3(target_dir[0], 0.0f, target_dir[2]));
		}
		else {
			event_input = true;
			target_position = float3(0,0,0);
		}
		
		physics_entity::physics_update();
	}
}

void physics_player::graphics_update() {
	// update cam
	if(!camera_control) return;
	physics_entity::graphics_update();
	const float3 pos(mdl->get_position());
	cam->set_position(-pos.x, -(pos.y + character_size.y), -pos.z);
}

void physics_player::block_step(const uint3& block) const {
	eevt->add_event(EVENT_TYPE::PLAYER_BLOCK_STEP, make_shared<player_block_step_event>(SDL_GetTicks(), block));
}

void physics_player::step(const float3& pos) const {
	eevt->add_event(EVENT_TYPE::PLAYER_STEP, make_shared<player_step_event>(SDL_GetTicks(), pos));
}

void physics_player::reset_parameters() {
	event_input = true;
	set_move_direction(float3(0.0f));
	reset_speed();
	target_position = float3(0.0f);
	timer_mseconds = SDL_GetTicks();

	health = 100.0f;
}

void physics_player::set_timer(int value) {
	timer_mseconds = value;
}

bool physics_player::event_handler(EVENT_TYPE type, shared_ptr<event_object> obj) {
	// if keyboard input flag is not set, return
	if(!event_input) return false;

	const pair<controls::ACTION, bool> action = controls::action_from_event(type, obj);
	switch(action.first) {
		case controls::ACTION::MOVE_RIGHT:
			move_state[0].store(action.second);
			break;
		case controls::ACTION::MOVE_LEFT:
			move_state[1].store(action.second);
			break;
		case controls::ACTION::MOVE_FORWARDS:
			move_state[2].store(action.second);
			break;
		case controls::ACTION::MOVE_BACKWARDS:
			move_state[3].store(action.second);
			break;
		case controls::ACTION::JUMP:
			jump_state.store(action.second);
			break;
		default: return false;
	}
	
	return true;
}

void physics_player::set_event_input(const bool& state) {
	event_input = state;
}

bool physics_player::get_event_input() const {
	return event_input;
}

void physics_player::set_camera_control(const bool& state) {
	camera_control = state;
}

bool physics_player::get_camera_control() const {
	return camera_control;
}

void physics_player::set_pulled(const bool& state) {
	is_pulled = state;
}

bool physics_player::get_pulled() const {
	return is_pulled;
}

void physics_player::set_health(const float& value) {
	health = value;
}

float physics_player::get_health() const {
	return health;
}
