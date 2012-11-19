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

#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h>
#include <BulletSoftBody/btDefaultSoftBodySolver.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include "physics_controller.h"
#include "rigid_body.h"
#include "soft_body.h"
#include "physics_player.h"
#include "physics_entity.h"
#include "weight_slider.h"

static constexpr float gravity = -9.81f;

physics_controller::physics_controller() : thread_base("physics"),
block_handler_fctr(this, &physics_controller::block_handler) {
	collision_configuration = new btSoftBodyRigidBodyCollisionConfiguration();
	dispatcher = new btCollisionDispatcher(collision_configuration);
	solver = new btSequentialImpulseConstraintSolver();
	overlapping_pair_cache = new btDbvtBroadphase();
	overlapping_pair_cache->getOverlappingPairCache()->setInternalGhostPairCallback(new btGhostPairCallback());
	
	soft_body_solver = new btDefaultSoftBodySolver();
	
	dynamics_world = new btSoftRigidDynamicsWorld(dispatcher, overlapping_pair_cache, solver, collision_configuration, soft_body_solver);
	dynamics_world->setGravity(get_global_bullet_gravity());
	
	soft_body_world_info = new btSoftBodyWorldInfo();
	soft_body_world_info->m_broadphase = overlapping_pair_cache;
	soft_body_world_info->m_dispatcher = dispatcher;
	soft_body_world_info->m_gravity.setValue(0.0f, gravity, 0.0f);
	soft_body_world_info->m_sparsesdf.Initialize();
	
	do_force_activate.test_and_set();
	eevt->add_event_handler(block_handler_fctr, EVENT_TYPE::BLOCK_CHANGE);
	
	this->set_thread_delay(5);
}

physics_controller::~physics_controller() {
	eevt->remove_event_handler(block_handler_fctr);
	
	// stop physics simulation before we destroy anything
	this->finish();
	
	// remove remaining constraints
	while(!sliders.empty()) {
		remove_weight_slider(sliders[0]);
	}
	
	// remove the rigid bodies from the dynamics world and delete them
	for(const auto& body : rigid_bodies) {
		dynamics_world->removeRigidBody(body->get_body());
		delete body;
	}
	rigid_bodies.clear();
	
	// delete rigid info objects
	for(const auto& info : rigid_infos) {
		delete info->construction_info;
		delete info->shape;
		delete info;
	}
	rigid_infos.clear();
	
	//
	delete dynamics_world;
	delete soft_body_world_info;
	delete soft_body_solver;
	delete solver;
	delete overlapping_pair_cache;
	delete dispatcher;
	delete collision_configuration;
}

void physics_controller::start_simulation() {
	prev_time_step = SDL_GetPerformanceCounter();
	enabled = true;
	this->start();
}

void physics_controller::halt_simulation() {
	lock();
	enabled = false;
	unlock();
}

void physics_controller::resume_simulation() {
	lock();
	enabled = true;
	prev_time_step = SDL_GetPerformanceCounter();
	total_sim_steps = 0;
	unlock();
}

void physics_controller::run() {
	if(!enabled) return;
	
	// check if level has changed -> make all dynamic physics bodies active
	if(!do_force_activate.test_and_set()) {
		force_active();
	}
	
	// run the simulation
	static const float perf_freq(SDL_GetPerformanceFrequency());
	const size_t cur_time_step = SDL_GetPerformanceCounter();
	const size_t sim_step_size = cur_time_step - prev_time_step;
	if(sim_step_size == 0) return;
	prev_time_step = cur_time_step;
	
	dynamics_world->stepSimulation(float(sim_step_size) / perf_freq, 20);
	total_sim_steps++;
	
	//
	for(const auto& entity : physics_entities) {
		entity->physics_update();
	}
	
	// at the beginning of the simulation sliders are highly unstable, so, to make sure
	// sliders don't get triggered because of this, wait for a couple of simulation steps
	static const size_t level_off = 32;
	if(total_sim_steps > level_off) {
		for(const auto& slider : sliders) {
			slider->update();
		}
	}
}

bool physics_controller::block_handler(EVENT_TYPE type, shared_ptr<event_object> obj a2e_unused) {
	if(type != EVENT_TYPE::BLOCK_CHANGE) return false;
	do_force_activate.clear();
	return true;
}

void physics_controller::update_models() {
	lock();
	
	// update models
	for(const auto& body : rigid_bodies) {
		body->update_model();
	}
	
	for(const auto& body : soft_bodies) {
		body->update_model();
	}
	
	unlock();
}

rigid_info& physics_controller::_add_rigid_info(btCollisionShape* shape, const float& mass) {
	lock();
	rigid_info* info = new rigid_info {
		mass,
		shape,
		nullptr
	};
	
	btVector3 local_inertia(0.0f, 0.0f, 0.0f);
	if(info->mass != 0.0f) {
		info->shape->calculateLocalInertia(info->mass, local_inertia);
	}
	
	info->construction_info = new btRigidBody::btRigidBodyConstructionInfo(info->mass, nullptr,
																		   info->shape, local_inertia);
	
	rigid_infos.push_back(info);
	unlock();
	return *info;
}

void physics_controller::remove_rigid_info(rigid_info* info) {
	lock();
	const auto iter = find(begin(rigid_infos), end(rigid_infos), info);
	if(iter != end(rigid_infos)) {
		rigid_infos.erase(iter);
		delete info;
	}
	unlock();
}

rigid_body& physics_controller::add_rigid_body(const rigid_info& rinfo, const float3& position, a2emodel* linked_mdl) {
	lock();
	rigid_body* rbody = new rigid_body(rinfo, position, linked_mdl);
	rigid_bodies.emplace_back(rbody);
	if(rinfo.mass > 0.0f) dynamic_rigid_bodies.emplace_back(rbody);
	dynamics_world->addRigidBody(rbody->get_body());
	unlock();
	return *rbody;
}

soft_body& physics_controller::add_soft_body(const string& filename, const float3& position, const soft_info& sinfo) {
	lock();
	soft_body* sbody = new soft_body(*soft_body_world_info, filename, position, sinfo);
	soft_bodies.emplace_back(sbody);
	dynamics_world->addSoftBody(sbody->get_body());
	unlock();
	return *sbody;
}

void physics_controller::remove_rigid_body(rigid_body* body) {
	lock();
	const auto iter = find(begin(rigid_bodies), end(rigid_bodies), body);
	if(iter != end(rigid_bodies)) {
		rigid_bodies.erase(iter);
		dynamics_world->removeRigidBody(body->get_body());
		delete body;
	}
	const auto dyn_iter = find(begin(dynamic_rigid_bodies), end(dynamic_rigid_bodies), body);
	if(dyn_iter != end(dynamic_rigid_bodies)) {
		dynamic_rigid_bodies.erase(dyn_iter);
	}
	unlock();
}

void physics_controller::remove_soft_body(soft_body* body) {
	lock();
	const auto iter = find(begin(soft_bodies), end(soft_bodies), body);
	if(iter != end(soft_bodies)) {
		soft_bodies.erase(iter);
		dynamics_world->removeSoftBody(body->get_body());
		delete body;
	}
	unlock();
}

void physics_controller::disable_body(rigid_body* body) {
	lock();
	dynamics_world->removeRigidBody(body->get_body());
	unlock();
}

void physics_controller::enable_body(rigid_body* body) {
	lock();
	dynamics_world->addRigidBody(body->get_body());
	unlock();
}

void physics_controller::force_active() {
	for(const auto& body : dynamic_rigid_bodies) {
		body->get_body()->setActivationState(ACTIVE_TAG);
	}
	for(const auto& body : soft_bodies) {
		body->get_body()->setActivationState(ACTIVE_TAG);
	}
}

const vector<rigid_body*>& physics_controller::get_rigid_bodies() const {
	return rigid_bodies;
}

const vector<rigid_body*>& physics_controller::get_dynamic_rigid_bodies() const {
	return dynamic_rigid_bodies;
}

const vector<soft_body*>& physics_controller::get_soft_bodies() const {
	return soft_bodies;
}

void physics_controller::make_dynamic(rigid_body& body, const float& mass) {
	lock();
	
	dynamics_world->removeRigidBody(body.get_body());
	
	btVector3 local_inertia(0.0f, 0.0f, 0.0f);
	body.get_body()->getCollisionShape()->calculateLocalInertia(mass, local_inertia);
	body.get_body()->setMassProps(mass, local_inertia);
	body.get_body()->setActivationState(ACTIVE_TAG);
	
	dynamics_world->addRigidBody(body.get_body());
	
	const auto dyn_iter = find(begin(dynamic_rigid_bodies), end(dynamic_rigid_bodies), &body);
	if(dyn_iter == end(dynamic_rigid_bodies)) {
		dynamic_rigid_bodies.push_back(&body);
	}
	
	unlock();
}

void physics_controller::make_static(rigid_body& body) {
	lock();
	
	dynamics_world->removeRigidBody(body.get_body());
	
	body.get_body()->setMassProps(0.0f, btVector3(0.0f, 0.0f, 0.0f));
	body.get_body()->setActivationState(ACTIVE_TAG);
	
	dynamics_world->addRigidBody(body.get_body());
	
	const auto dyn_iter = find(begin(dynamic_rigid_bodies), end(dynamic_rigid_bodies), &body);
	if(dyn_iter != end(dynamic_rigid_bodies)) {
		dynamic_rigid_bodies.erase(dyn_iter);
	}
	
	unlock();
}

void physics_controller::add_physics_entity(physics_entity& entity) {
	lock();
	physics_entities.push_back(&entity);
	unlock();
}

void physics_controller::remove_physics_entity(const physics_entity& entity) {
	lock();
	const auto iter = find(begin(physics_entities), end(physics_entities), &entity);
	if(iter != end(physics_entities)) {
		physics_entities.erase(iter);
	}
	unlock();
}

const vector<physics_entity*>& physics_controller::get_physics_entities() const {
	return physics_entities;
}

weight_slider* physics_controller::add_weight_slider(const float3& position, const float& height, const float& mass) {
	lock();
	weight_slider* slider = new weight_slider(position, height, mass);
	dynamics_world->addConstraint(slider->get_constraint(), true);
	sliders.push_back(slider);
	unlock();
	return slider;
}

void physics_controller::remove_weight_slider(weight_slider* slider) {
	lock();
	const auto iter = find(begin(sliders), end(sliders), slider);
	if(iter != end(sliders)) {
		sliders.erase(iter);
		dynamics_world->removeConstraint(slider->get_constraint());
		delete slider;
	}
	unlock();
}

float physics_controller::get_global_gravity() {
	return gravity;
}

btVector3 physics_controller::get_global_bullet_gravity() {
	return btVector3(0.0f, gravity, 0.0f);
}
