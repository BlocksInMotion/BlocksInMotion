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

#ifndef __SB_PHYSICS_CONTROLLER_H__
#define __SB_PHYSICS_CONTROLLER_H__

#include <BulletDynamics/btBulletDynamicsCommon.h>
#include "sb_global.h"
#include <threading/thread_base.h>
#include <scene/model/a2emodel.h>
#include <atomic>

class btSoftBodyRigidBodyCollisionConfiguration;
class btSoftRigidDynamicsWorld;
class btSoftBodySolver;
struct btSoftBodyWorldInfo;
struct rigid_info;
class rigid_body;
struct soft_info;
class soft_body;
class physics_player;
class physics_entity;
class weight_slider;
class physics_controller : public thread_base {
public:
	physics_controller();
	virtual ~physics_controller();
	
	enum class SHAPE : unsigned int {
		BOX,
		SPHERE,
		CYLINDER,
		CAPSULE,
		CONE,
		BVH_TRIANGLE_MESH,
		__MAX_SHAPE
	};
	
	virtual void run();
	void start_simulation();
	void halt_simulation();
	void resume_simulation();
	
	// this should be called from the main/render loop
	void update_models();
	
	// note: a mass of 0.0f means the object is fixed
	// note: BVH_TRIANGLE_MESH can only be fixed
	template<SHAPE shape, typename... Args> rigid_info& add_rigid_info(const float& mass, const Args&... args);
	void remove_rigid_info(rigid_info* info);
	
	//
	rigid_body& add_rigid_body(const rigid_info& rinfo, const float3& position, a2emodel* linked_mdl = nullptr);
	soft_body& add_soft_body(const string& filename, const float3& position, const soft_info& sinfo);
	void remove_rigid_body(rigid_body* body);
	void remove_soft_body(soft_body* body);
	
	void make_dynamic(rigid_body& body, const float& mass);
	void make_static(rigid_body& body);
	
	const vector<rigid_body*>& get_rigid_bodies() const;
	const vector<rigid_body*>& get_dynamic_rigid_bodies() const;
	const vector<soft_body*>& get_soft_bodies() const;
	
	//
	weight_slider* add_weight_slider(const float3& position, const float& height, const float& mass);
	void remove_weight_slider(weight_slider* slider);
	
	//
	void add_physics_entity(physics_entity& entity);
	void remove_physics_entity(const physics_entity& entity);
	const vector<physics_entity*>& get_physics_entities() const;

	static float get_global_gravity();
	static btVector3 get_global_bullet_gravity();

	void disable_body(rigid_body* body);
	void enable_body(rigid_body* body);
	
protected:
	// global/world data
	btSoftBodyRigidBodyCollisionConfiguration* collision_configuration = nullptr;
	btCollisionDispatcher* dispatcher = nullptr;
	btBroadphaseInterface* overlapping_pair_cache = nullptr;
	btSequentialImpulseConstraintSolver* solver = nullptr;
	btSoftBodySolver* soft_body_solver = nullptr;
	btSoftRigidDynamicsWorld* dynamics_world = nullptr;
	btSoftBodyWorldInfo* soft_body_world_info = nullptr;
	
	// rigid body data
	vector<rigid_body*> rigid_bodies;
	vector<rigid_body*> dynamic_rigid_bodies;
	vector<rigid_info*> rigid_infos;
	rigid_info& _add_rigid_info(btCollisionShape* shape, const float& mass);
	template <const SHAPE shape> struct shape_maker {
		template<typename... Args> static btCollisionShape* create_shape(const Args&... args);
	};
	
	vector<physics_entity*> physics_entities;
	
	// soft body data
	vector<soft_body*> soft_bodies;
	
	//
	event::handler block_handler_fctr;
	bool block_handler(EVENT_TYPE type, shared_ptr<event_object> obj);
	
	atomic_flag do_force_activate = ATOMIC_FLAG_INIT; // true = no, false = yes
	void force_active();
	
	//
	size_t prev_time_step = 0;
	bool enabled = false;
	size_t total_sim_steps = 0;
	
	// constraints
	vector<weight_slider*> sliders;

};

template<physics_controller::SHAPE shape, typename... Args> rigid_info& physics_controller::add_rigid_info(const float& mass, const Args&... args) {
	return _add_rigid_info(shape_maker<shape>::create_shape(args...), mass);
}

template <> struct physics_controller::shape_maker<physics_controller::SHAPE::BOX> {
	static btCollisionShape* create_shape(const float3& half_extents) {
		return new btBoxShape(btVector3(half_extents.x, half_extents.y, half_extents.z));
	}
};

template <> struct physics_controller::shape_maker<physics_controller::SHAPE::SPHERE> {
	static btCollisionShape* create_shape(const float& radius) {
		return new btSphereShape(radius);
	}
};

template <> struct physics_controller::shape_maker<physics_controller::SHAPE::CYLINDER> {
	static btCollisionShape* create_shape(const float3& half_extents) {
		return new btCylinderShape(btVector3(half_extents.x, half_extents.y, half_extents.z));
	}
};

template <> struct physics_controller::shape_maker<physics_controller::SHAPE::CAPSULE> {
	static btCollisionShape* create_shape(const float& radius, const float& half_height) {
		return new btCapsuleShape(radius, half_height);
	}
};

template <> struct physics_controller::shape_maker<physics_controller::SHAPE::CONE> {
	static btCollisionShape* create_shape(const float& radius, const float& half_height) {
		return new btConeShape(radius, half_height);
	}
};

template <> struct physics_controller::shape_maker<physics_controller::SHAPE::BVH_TRIANGLE_MESH> {
	static btCollisionShape* create_shape(const a2emodel* model) {
		btTriangleIndexVertexArray* mesh = new btTriangleIndexVertexArray(model->get_index_count(0),
																		  (int*)model->get_indices(0),
																		  sizeof(index3),
																		  model->get_vertex_count(0),
																		  (btScalar*)model->get_vertices(0),
																		  sizeof(float3));
		return new btBvhTriangleMeshShape(mesh, true);
	}
};

#endif
