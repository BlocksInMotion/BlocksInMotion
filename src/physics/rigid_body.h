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

#ifndef __SB_RIGID_BODY_H__
#define __SB_RIGID_BODY_H__

#include "sb_global.h"
#include <core/bbox.h>
#include <BulletDynamics/btBulletDynamicsCommon.h>

// NOTE: reuse *CollisionShape and *ConstructionInfo object whenever possible (-> performance)
struct rigid_info {
	const float mass;
	btCollisionShape* shape;
	btRigidBody::btRigidBodyConstructionInfo* construction_info;
};

class a2emodel;
class rigid_body {
public:
	rigid_body(const rigid_info& rinfo, const float3& position, a2emodel* linked_mdl = nullptr);
	~rigid_body();
	
	void update_model();
	
	btRigidBody* get_body();
	a2emodel* get_linked_model();
	
	void set_position(const float3& position);
	const float3& get_position() const;
	void set_scale(const float3& scale);
	const float3& get_scale() const;

	matrix4f get_rotation() const;

	void compute_bbox(bbox& result) const;
	
protected:
	const rigid_info& rinfo;
	btRigidBody* body;
	btDefaultMotionState* motion_state;
	a2emodel* linked_mdl;

	float3 position;
	float3 scale = float3(1.0f);

};

#endif
