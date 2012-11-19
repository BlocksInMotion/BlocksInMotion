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

#include "physics_entity.h"
#include "physics_controller.h"
#include "sb_map.h"
#include <engine.h>
#include <scene/scene.h>
#include <scene/camera.h>

static constexpr float step_length = 1.5f;

physics_entity::physics_entity(const float3& position, const float2 character_size_, const string model_filename, const string material_filename) :
character_size(character_size_) {
	//
	mat = new a2ematerial(e);
	mat->load_material(e->data_path(material_filename.empty() ? "sphere.a2mtl" : material_filename));
	
	mdl = sce->create_a2emodel<a2estatic>();
	mdl->load_model(e->data_path(model_filename.empty() ? "cylinder.a2m" : model_filename));
	mdl->set_material(mat);
	mdl->set_position(position);
	mdl->set_hard_scale(character_size.x, character_size.y, character_size.x);
	sce->add_model(mdl);
	
	//
	pc->lock();
	character_rinfo = &pc->add_rigid_info<physics_controller::SHAPE::CAPSULE>(10.0f, character_size.x, character_size.y);
	character_rinfo->construction_info->m_friction = 0.001f; // no friction -> no sticking to walls
	character_body = &pc->add_rigid_body(*character_rinfo, position, mdl);
	body = character_body->get_body();
	body->setSleepingThresholds(0.0f, 0.0f);
	body->setAngularFactor(0.0f);
	pc->add_physics_entity(*this);
	pc->unlock();
}

physics_entity::~physics_entity() {
	pc->remove_physics_entity(*this);
	if(mdl != nullptr) {
		sce->delete_model(mdl);
		delete mdl;
	}
	if(mat != nullptr) delete mat;
	
	pc->remove_rigid_body(character_body);
}

void physics_entity::physics_update() {
	const btVector3 cur_lvel(body->getLinearVelocity() * btVector3(prev_velocity_scale, 1.0f, prev_velocity_scale));
	float3 velocity = move_direction * speed;

	// combined x/z velocity should never exceed speed (for velocity increases in here!)
	const float cur_xz_speed = fabs(cur_lvel.x()) + fabs(cur_lvel.z());
	if((fabs(velocity.x) + fabs(velocity.z) + cur_xz_speed) > speed) {
		if(cur_xz_speed < speed) {
			float2 norm_xz_vel(velocity.x, velocity.z);
			norm_xz_vel.normalize();
			const float rem_speed = speed - cur_xz_speed;
			velocity.x = norm_xz_vel.x * rem_speed;
			velocity.z = norm_xz_vel.y * rem_speed;
		}
		else {
			// ignore new velocity
			velocity.x = 0.0f;
			velocity.z = 0.0f;
		}
	}
	velocity += float3(cur_lvel.x(), cur_lvel.y(), cur_lvel.z());
	body->setLinearVelocity(btVector3(velocity.x, velocity.y, velocity.z));
	cur_velocity = velocity;
}

void physics_entity::graphics_update() {
	const float3 pos(mdl->get_position());
	const uint3 cur_block(float3(pos.x, pos.y - character_size.y * 0.5f, pos.z).floored());
	if((cur_block != prev_block).any()) {
		prev_block = cur_block;
		// try not to add events for invalid block positions
		if(active_map == nullptr ||
		   (active_map != nullptr && active_map->is_valid_position(cur_block))) {
			block_step(cur_block);
		}
	}
	
	//
	static constexpr float jitter_epsilon = 0.01f;
	const float3 pos_diff(pos - prev_pos);
	const float step_advance = fabs(pos_diff.x) + fabs(pos_diff.z);
	prev_pos = pos;
	// account for jittering and flying
	cur_step_length += (step_advance > jitter_epsilon && fabs(pos_diff.y) < jitter_epsilon ?
						step_advance : 0.0f);
	if(cur_step_length >= step_length) {
		cur_step_length -= step_length;
		step(pos);
	}
}

void physics_entity::block_step(const uint3& block a2e_unused) const {
}

void physics_entity::step(const float3& pos a2e_unused) const {
}

void physics_entity::set_position(const float3& position) {
	pc->lock();
	character_body->set_position(position);
	body->setLinearVelocity(btVector3(0.0f, 0.0f, 0.0f));
	pc->unlock();
}

const float3& physics_entity::get_position() const {
	return character_body->get_position();
}

void physics_entity::set_rotation(const float3& rotation) {
	mdl->set_rotation(rotation);
}

void physics_entity::set_move_direction(const float3& direction) {
	move_direction = direction;
}

const float3& physics_entity::get_move_direction() const {
	return move_direction;
}

void physics_entity::set_speed(const float& speed_) {
	speed = speed_;
}

const float& physics_entity::get_speed() const {
	return speed;
}

void physics_entity::reset_speed() {
	speed = default_speed;
}

void physics_entity::set_jump_strength(const float& strength) {
	jump_strength = strength;
}

const float& physics_entity::get_jump_strength() const {
	return jump_strength;
}

void physics_entity::reset_jump_strength() {
	jump_strength = default_jump_strength;
}

rigid_info* physics_entity::get_character_info() {
	return character_rinfo;
}

rigid_body* physics_entity::get_character_body() {
	return character_body;
}
