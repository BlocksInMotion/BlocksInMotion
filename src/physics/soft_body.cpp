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

#include <BulletSoftBody/btSoftBody.h>
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <BulletSoftBody/btSoftBodyHelpers.h>
#include "soft_body.h"
#include <scene/scene.h>

soft_body::soft_body(btSoftBodyWorldInfo& world_info, const string& filename, const float3& position, const soft_info& sinfo) :
a2estatic(::e, ::s, ::sce), construction_info(sinfo)
{
	load_model(filename);
	
	body = btSoftBodyHelpers::CreateFromTriMesh(world_info,
												(const btScalar*)vertices,
												(const int*)indices[0],
												index_count[0]);

	body->m_cfg.piterations = 10;
	
	if(!isnan(sinfo.kVCF)) body->m_cfg.kVCF = sinfo.kVCF;
	if(!isnan(sinfo.kDP)) body->m_cfg.kDP = sinfo.kDP;
	if(!isnan(sinfo.kDG)) body->m_cfg.kDG = sinfo.kDG;
	if(!isnan(sinfo.kLF)) body->m_cfg.kLF = sinfo.kLF;
	if(!isnan(sinfo.kPR)) body->m_cfg.kPR = sinfo.kPR;
	if(!isnan(sinfo.kVC)) body->m_cfg.kVC = sinfo.kVC;
	if(!isnan(sinfo.kDF)) body->m_cfg.kDF = sinfo.kDF;
	if(!isnan(sinfo.kMT)) body->m_cfg.kMT = sinfo.kMT;
	if(!isnan(sinfo.kCHR)) body->m_cfg.kCHR = sinfo.kCHR;
	if(!isnan(sinfo.kKHR)) body->m_cfg.kKHR = sinfo.kKHR;
	if(!isnan(sinfo.kSHR)) body->m_cfg.kSHR = sinfo.kSHR;
	if(!isnan(sinfo.kAHR)) body->m_cfg.kAHR = sinfo.kAHR;
	
	body->randomizeConstraints();
	body->setPose(sinfo.pose_volume, sinfo.pose_bframe);
	body->setTotalMass(sinfo.mass, true);
	
	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(btVector3(position.x, position.y, position.z));
	body->transform(transform);
	
	sce->add_model(this);
}

soft_body::~soft_body() {
	sce->delete_model(this);
	if(body != nullptr) delete body;
}

btSoftBody* soft_body::get_body() {
	return body;
}

void soft_body::update_model() {
	// update vertices
	btSoftBody::tNodeArray& nodes(body->m_nodes);
	avg_position = float3(0.0f);
	for(int i = 0; i < nodes.size(); i++) {
		vertices[i].set(nodes[i].m_x.x(), nodes[i].m_x.y(), nodes[i].m_x.z());
		normals[i].set(nodes[i].m_n.x(), nodes[i].m_n.y(), nodes[i].m_n.z());
		avg_position += vertices[i];
	}
	avg_position /= nodes.size();
	
	glBindBuffer(GL_ARRAY_BUFFER, vbo_vertices_id);
	glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_count * sizeof(float3), vertices);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_normals_id);
	glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_count * sizeof(float3), normals);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void soft_body::draw(const DRAW_MODE draw_mode) {
	const DRAW_MODE masked_draw_mode((DRAW_MODE)((unsigned int)draw_mode & (unsigned int)DRAW_MODE::GM_PASSES_MASK));
	if(masked_draw_mode == DRAW_MODE::GEOMETRY_ALPHA_PASS ||
	   masked_draw_mode == DRAW_MODE::MATERIAL_ALPHA_PASS) {
		return; // no alpha for now
	}
	
	// draw
	pre_draw_setup();
	
	// vbo setup
	draw_vertices_vbo = vbo_vertices_id;
	draw_tex_coords_vbo = vbo_tex_coords_id;
	draw_normals_vbo = vbo_normals_id;
	draw_binormals_vbo = vbo_binormals_id;
	draw_tangents_vbo = vbo_tangents_id;
	draw_indices_vbo = vbo_indices_ids[0];
	draw_index_count = index_count[0] * 3;
	draw_sub_object(draw_mode, 0, 0);
	
	post_draw_setup();
}

const float3& soft_body::get_avg_position() const {
	return avg_position;
}

const soft_info& soft_body::get_construction_info() const {
	return construction_info;
}
