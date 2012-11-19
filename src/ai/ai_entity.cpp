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

#include "ai_entity.h"
#include "physics_controller.h"
#include "game.h"
#include "map_renderer.h"
#include "sb_map.h"
#include <particle/particle.h>

ai_entity::ai_entity(const float3& position) :
physics_entity(position, float2(0.5f, 0.5f), "spheroid.a2m", "ai.a2mtl"),
waypoints(active_map->get_ai_waypoints())
{
	speed = 2.0f;
	default_speed = speed;
	
	avg_rotation.fill(0.0f);

	timer_mseconds = SDL_GetTicks();

	// find nearest viable waypoint
	static constexpr float max_waypoint_distance { 6.0f };
	pair<float, sb_map::ai_waypoint*> min_wp { max_waypoint_distance + 1.0f, nullptr };
	for(const auto& wp : waypoints) {
		const float dist = position.distance(wp->position);
		if(dist <= max_waypoint_distance && dist < min_wp.first) {
			min_wp.first = dist;
			min_wp.second = wp;
		}
	}
	if(min_wp.second != nullptr) {
		cur_waypoint = min_wp.second;
		move_to_position(cur_waypoint->position);
		target_mode = AI_TARGET_MODE::WAYPOINT;
	}
	
	//
	if(pm != nullptr) {
		auto tex = mr->get_particle_texture("AI_ATTACK");
		if(tex != nullptr) {
			attack_ps = pm->add_particle_system(particle_system::EMITTER_TYPE::SPHERE,
												particle_system::LIGHTING_TYPE::NONE,
												tex,
												8192,
												500,
												5.0f,
												float3(0.0f),
												float3(0.0f),
												float3(0.1f),
												float3(0.0f, 1.0, 0.0f),
												float3(0.0f),
												float3(0.0f, 0.0f, 0.0f),
												float4(0.85f, 0.05f, 0.05f, 0.125f),
												float2(0.075f, 0.075f));
			attack_ps->set_active(false);
			attack_ps->set_visible(false);
		}
	}
}

ai_entity::~ai_entity() {
	if(attack_ps != nullptr) {
		pm->delete_particle_system(attack_ps);
		attack_ps = nullptr;
	}
}

void ai_entity::physics_update() {
	float3 target_dir = target_position - get_position();
	const float len = target_dir.length();
	if (!target_position.is_null() && (len > 2.0f)) {
		// speed up body -> min scale 3
		target_dir = target_dir.normalized() * std::max<float>(len, 0.5f);
		character_body->get_body()->setLinearVelocity(btVector3(target_dir[0], 0.0f, target_dir[2]));
	}
	else {
		target_position.set(0.0f, 0.0f, 0.0f);
		
		if (target_mode != AI_TARGET_MODE::WAIT) {
			sense();
		}
		
		switch(target_mode) {
			// patrol mode
			case AI_TARGET_MODE::PATROL:
				do_patrolling();
				break;
		
			// follow mode
			case AI_TARGET_MODE::FOLLOW:
				move_to_position(ge->get_position());
				break;
				
			// tractor mode
			case AI_TARGET_MODE::ATTACK:
				force_push_player();
				break;
		
			case AI_TARGET_MODE::WAIT:
				stop_moving();
				wait(5.0f);
				break;

			case AI_TARGET_MODE::NONE:
				reset_parameters();
				break;
				
			case AI_TARGET_MODE::WAYPOINT:
				check_waypoint();
				break;
		}
	}
	
	physics_entity::physics_update();
}

void ai_entity::graphics_update() {
	// set/compute ai rotation (averaged over multiple frames)
	const float2 norm_dir(float2(move_direction.x, move_direction.z).normalized());
	if(!norm_dir.is_null() && !norm_dir.is_nan()) {
		avg_rotation[avg_rotation_index] = norm_dir;
		avg_rotation_index = (avg_rotation_index + 1) % avg_rotation.size();
		
		float2 avg_rot(0.0f, 0.0f);
		for(const auto& rot : avg_rotation) {
			avg_rot += rot;
		}
		avg_rot.normalize();
		
		const float cur_rot = core::wrap(fabs(RAD2DEG(acosf(avg_rot.dot(float2(0.0f, -1.0f))))
											  + (avg_rot.x < 0.0f ? -360.0f : 0.0f)
											  + 90.0f),
										 360.0f);
		set_rotation(float3(0.0f, cur_rot, 0.0f));
	}
	
	// handle attack particle system deactivtion
	static constexpr unsigned int attack_ps_duration = 400;
	if(attack_ps != nullptr && attack_ps_timer > 0 &&
	   (SDL_GetTicks() - attack_ps_timer) > attack_ps_duration) {
		attack_ps_timer = 0;
		attack_ps->set_active(false);
		attack_ps->set_visible(false);
	}
	
	physics_entity::graphics_update();
}

void ai_entity::block_step(const uint3& block) const {
	eevt->add_event(EVENT_TYPE::AI_BLOCK_STEP, make_shared<ai_block_step_event>(SDL_GetTicks(), block, this));
}

void ai_entity::step(const float3& pos) const {
	eevt->add_event(EVENT_TYPE::AI_STEP, make_shared<ai_step_event>(SDL_GetTicks(), pos));
}

void ai_entity::turn_back() {
	set_move_direction(-get_move_direction());
}

float ai_entity::get_distance_to_player() {
	return (ge->get_position() - get_position()).length();
}

void ai_entity::sense() {
	const float dist = get_distance_to_player();
	if(dist < sensor_distance) {
		if(dist < attack_distance && ge->is_in_line_of_sight(get_position(), attack_distance)) {
			target_mode = AI_TARGET_MODE::ATTACK;
		}
		else {
			target_mode = AI_TARGET_MODE::FOLLOW;
		}
	}
	else if(target_mode != AI_TARGET_MODE::WAYPOINT) {
		target_mode = AI_TARGET_MODE::PATROL;
	}
}

void ai_entity::do_patrolling() {
	// possible move_directions
	static a2e_constexpr array<float3, 7> move_array {
		{
			float3(-1.0f, 0.0f, -1.0f),
			float3(-1.0f, 0.0f, 0.0f),
			float3(0.0f, 0.0f, -1.0f),
			float3(0.0f, 0.0f, 0.0f),
			float3(0.0f, 0.0f, 1.0f),
			float3(1.0f, 0.0f, 0.0f),
			float3(1.0f, 0.0f, 1.0f)
		}
	};
	
	// possibilities for each direction
	static constexpr float chance { 0.005f };
	static a2e_constexpr array<float, 7> move_array_p {
		{
			chance,
			chance,
			chance,
			0.0f,
			chance,
			chance,
			chance
		}
	};
	
	
	// possibility which direction to choose
	const float p = core::rand(0.0f, 1.0f);
	const unsigned int r = core::rand(0, 7);
	
	// check if object will change direction
	if (p < move_array_p[r]) {
		set_move_direction(move_array[r]);
	}
	
	// turn back with a certain possibility
	if(core::rand(0.0f, 1.0f) < chance) {
		turn_back();
	}
}

void ai_entity::force_push_ai() {
	const float3 d = (get_position() - ge->get_position()).normalized();
	const float3 p = get_position();
	target_position.set((d.x * 5.0f + p.x), get_position().y, (d.z * 5.0f + p.z));
}

void ai_entity::force_pull_player() {
	stop_moving();

	float3 dir(get_position() - ge->get_position());
	if (dir.length() <= 2.0f) {
		dir.set(0.0f, 0.0f, 0.0f);
		ge->set_event_input(true);
	} else {
		dir.normalize();
		ge->set_event_input(false);
	}
	ge->set_move_direction(dir);
}

void ai_entity::force_push_player() {
	ge->damage(weapon_strength);
	timer_mseconds = SDL_GetTicks();
	
	target_mode = AI_TARGET_MODE::WAIT;
	
	// start attack particle system
	if(attack_ps != nullptr) {
		attack_ps_timer = SDL_GetTicks();
		attack_ps->set_position(mdl->get_position());
		attack_ps->set_active(true);
		attack_ps->set_visible(true);
		attack_ps->set_direction((ge->get_position() - get_position() + float3(0.0f, 0.5f, 0.0f)).normalized());
	}
}

void ai_entity::wait(const unsigned int& seconds) {
	if((SDL_GetTicks() - timer_mseconds) > (seconds * 1000)) {
		timer_mseconds = SDL_GetTicks();
		target_mode = AI_TARGET_MODE::PATROL;
	}
	else {
		target_mode = AI_TARGET_MODE::WAIT;
	}
}

void ai_entity::stop_moving() {
	set_move_direction(float3(0.0f));
}

void ai_entity::check_waypoint() {
	if(cur_waypoint == nullptr) return;
	if(float3(cur_waypoint->position).distance(get_position()) < 1.0f) {
		next_waypoint();
	}
}

void ai_entity::next_waypoint() {
	if(cur_waypoint == nullptr) return;
	
	cur_waypoint = cur_waypoint->next;
	if(cur_waypoint != nullptr) {
		move_to_position(cur_waypoint->position);
	}
	else {
		// -> free movement again
		sense();
	}
}

void ai_entity::move_to_position(const float3& pos) {
	move_position = pos;
	calc_move_direction(get_position(), move_position);
}

void ai_entity::calc_move_direction(const float3& from_pos, const float3& to_pos) {
	float3 dir(to_pos - from_pos);
	if(dir.length() <= 0.3f) {
		dir.set(0.0f, 0.0f, 0.0f);
	}
	else {
		dir.normalize();
	}
	set_move_direction(dir);
}

bool ai_entity::is_patrolling() const {
	return (target_mode == AI_TARGET_MODE::PATROL) ? true : false;
}

bool ai_entity::is_attacking() const {
	return (target_mode == AI_TARGET_MODE::ATTACK) ? true : false;
}

bool ai_entity::is_following() const {
	return (target_mode == AI_TARGET_MODE::FOLLOW) ? true : false;
}

void ai_entity::reset_parameters() {
	set_move_direction(float3(0.0f));
	reset_speed();
	target_position.set(0.0f, 0.0f, 0.0f);
}
