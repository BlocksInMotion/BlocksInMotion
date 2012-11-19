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

#include "rigid_body.h"
#include "physics_controller.h"
#include <scene/model/a2emodel.h>

rigid_body::rigid_body(const rigid_info& rinfo_, const float3& position_, a2emodel* linked_mdl_) :
rinfo(rinfo_),
#if defined(__clang__)
// ignore bullet alignment failure
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wover-aligned"
#endif
body(new btRigidBody(*rinfo_.construction_info)),
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
motion_state(nullptr),
linked_mdl(linked_mdl_),
position(position_)
{
	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(btVector3(position.x, position.y, position.z));
	motion_state = new btDefaultMotionState(transform);
	body->setMotionState(motion_state);
}

rigid_body::~rigid_body() {
	if(motion_state != nullptr) delete motion_state;
	if(body != nullptr) delete body;
}

void rigid_body::update_model() {
	// update position of all rigid bodies with a mass (for non-mass bodies the postion is always the initial one)
	btTransform trans;
	if(body->getInvMass() > 0.0f || linked_mdl != nullptr) {
		motion_state->getWorldTransform(trans);
		position.set(trans.getOrigin().getX(), trans.getOrigin().getY(), trans.getOrigin().getZ());
	}
	
	if(linked_mdl == nullptr) return;

	// copy the rotation matrix values directly
	const auto& rmat(trans.getBasis());
	auto& mdl_rmat(linked_mdl->get_rotation_matrix());
	mdl_rmat[0] = rmat[0][0];
	mdl_rmat[1] = rmat[1][0];
	mdl_rmat[2] = rmat[2][0];
	mdl_rmat[4] = rmat[0][1];
	mdl_rmat[5] = rmat[1][1];
	mdl_rmat[6] = rmat[2][1];
	mdl_rmat[8] = rmat[0][2];
	mdl_rmat[9] = rmat[1][2];
	mdl_rmat[10] = rmat[2][2];
	
	linked_mdl->set_position(position);
}

btRigidBody* rigid_body::get_body() {
	return body;
}

a2emodel* rigid_body::get_linked_model() {
	return linked_mdl;
}

void rigid_body::set_position(const float3& position_) {
	pc->lock();
	position = position_;
	body->getWorldTransform().setOrigin(btVector3(position.x, position.y, position.z));
	pc->unlock();
	if(linked_mdl != nullptr) linked_mdl->set_position(position);
}

const float3& rigid_body::get_position() const {
	return position;
}

void rigid_body::set_scale(const float3& scale_) {
	pc->lock();
	scale = scale_;
	body->getCollisionShape()->setLocalScaling(btVector3(scale.x, scale.y, scale.z));
	pc->unlock();
}

const float3& rigid_body::get_scale() const {
	return scale;
}

matrix4f rigid_body::get_rotation() const {
	pc->lock();
	btTransform trans;
	motion_state->getWorldTransform(trans);
	const auto& source_rot_matrix(trans.getBasis());
	matrix4f rot_matrix;
	rot_matrix[0] = source_rot_matrix[0][0];
	rot_matrix[1] = source_rot_matrix[1][0];
	rot_matrix[2] = source_rot_matrix[2][0];
	rot_matrix[4] = source_rot_matrix[0][1];
	rot_matrix[5] = source_rot_matrix[1][1];
	rot_matrix[6] = source_rot_matrix[2][1];
	rot_matrix[8] = source_rot_matrix[0][2];
	rot_matrix[9] = source_rot_matrix[1][2];
	rot_matrix[10] = source_rot_matrix[2][2];
	pc->unlock();
	return rot_matrix;
}

void rigid_body::compute_bbox(bbox& result) const {
	pc->lock();
	btTransform trans;
	motion_state->getWorldTransform(trans);
	btVector3 min, max;
	body->getCollisionShape()->getAabb(trans, min, max);
	result = bbox(float3(min.x(), min.y(), min.z()), float3(max.x(), max.y(), max.z()));
	pc->unlock();
}
