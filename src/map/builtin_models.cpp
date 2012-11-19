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

#include "builtin_models.h"
#include "sb_map.h"

// built-in types/data here:
static constexpr size_t culling_shift = 16;
const vector<float3> builtin_models::cube_vertices {
	float3(0.0f, 0.0f, 0.0f), //0 bottom
	float3(1.0f, 0.0f, 0.0f),
	float3(1.0f, 0.0f, 1.0f),
	float3(0.0f, 0.0f, 1.0f),
	
	float3(0.0f, 1.0f, 0.0f), //4 top
	float3(1.0f, 1.0f, 0.0f),
	float3(1.0f, 1.0f, 1.0f),
	float3(0.0f, 1.0f, 1.0f),
	
	float3(0.0f, 0.0f, 0.0f), //8 front
	float3(1.0f, 0.0f, 0.0f),
	float3(1.0f, 1.0f, 0.0f),
	float3(0.0f, 1.0f, 0.0f),
	
	float3(1.0f, 0.0f, 0.0f), //12 right
	float3(1.0f, 0.0f, 1.0f),
	float3(1.0f, 1.0f, 1.0f),
	float3(1.0f, 1.0f, 0.0f),
	
	float3(1.0f, 0.0f, 1.0f), //16 back
	float3(0.0f, 0.0f, 1.0f),
	float3(0.0f, 1.0f, 1.0f),
	float3(1.0f, 1.0f, 1.0f),
	
	float3(0.0f, 0.0f, 1.0f), //20 left
	float3(0.0f, 0.0f, 0.0f),
	float3(0.0f, 1.0f, 0.0f),
	float3(0.0f, 1.0f, 1.0f),
};
const vector<unsigned int> builtin_models::cube_culling {
	(unsigned int)BLOCK_FACE::BOTTOM << culling_shift, //0 bottom
	(unsigned int)BLOCK_FACE::BOTTOM << culling_shift,
	(unsigned int)BLOCK_FACE::BOTTOM << culling_shift,
	(unsigned int)BLOCK_FACE::BOTTOM << culling_shift,
	
	(unsigned int)BLOCK_FACE::TOP << culling_shift, //4 top
	(unsigned int)BLOCK_FACE::TOP << culling_shift,
	(unsigned int)BLOCK_FACE::TOP << culling_shift,
	(unsigned int)BLOCK_FACE::TOP << culling_shift,
	
	(unsigned int)BLOCK_FACE::FRONT << culling_shift, //8 front
	(unsigned int)BLOCK_FACE::FRONT << culling_shift,
	(unsigned int)BLOCK_FACE::FRONT << culling_shift,
	(unsigned int)BLOCK_FACE::FRONT << culling_shift,
	
	(unsigned int)BLOCK_FACE::RIGHT << culling_shift, //12 right
	(unsigned int)BLOCK_FACE::RIGHT << culling_shift,
	(unsigned int)BLOCK_FACE::RIGHT << culling_shift,
	(unsigned int)BLOCK_FACE::RIGHT << culling_shift,
	
	(unsigned int)BLOCK_FACE::BACK << culling_shift, //16 back
	(unsigned int)BLOCK_FACE::BACK << culling_shift,
	(unsigned int)BLOCK_FACE::BACK << culling_shift,
	(unsigned int)BLOCK_FACE::BACK << culling_shift,
	
	(unsigned int)BLOCK_FACE::LEFT << culling_shift, //20 left
	(unsigned int)BLOCK_FACE::LEFT << culling_shift,
	(unsigned int)BLOCK_FACE::LEFT << culling_shift,
	(unsigned int)BLOCK_FACE::LEFT << culling_shift,
};
const vector<uchar3> builtin_models::cube_indices {
	uchar3(0, 1, 2),
	uchar3(0, 2, 3),
	
	uchar3(4, 6, 5),
	uchar3(4, 7, 6),
	
	uchar3(8, 10, 9),
	uchar3(8, 11, 10),
	
	uchar3(12, 14, 13),
	uchar3(12, 15, 14),
	
	uchar3(16, 18, 17),
	uchar3(16, 19, 18),
	
	uchar3(20, 22, 21),
	uchar3(20, 23, 22),
};
const vector<coord> builtin_models::cube_tex_coords {
	float2(0.0f, 1.0f),
	float2(1.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 0.0f),
	
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f),
	float2(0.0f, 1.0f),
	
	float2(1.0f, 1.0f),
	float2(0.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	
	float2(1.0f, 1.0f),
	float2(0.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	
	float2(1.0f, 1.0f),
	float2(0.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	
	float2(1.0f, 1.0f),
	float2(0.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
};
vector<float3> builtin_models::cube_normals;
vector<float3> builtin_models::cube_binormals;
vector<float3> builtin_models::cube_tangents;

const size_t builtin_models::cube_vertex_count = builtin_models::cube_vertices.size();
const size_t builtin_models::cube_index_count = builtin_models::cube_indices.size();

static bool builtin_models_initialized = false;
void builtin_models::init() {
	if(builtin_models_initialized) return;
	builtin_models_initialized = true;
	
	// compute normals
	// create zero normals
	builtin_models::cube_normals.resize(cube_vertex_count, float3());
	builtin_models::cube_binormals.resize(cube_vertex_count, float3());
	builtin_models::cube_tangents.resize(cube_vertex_count, float3());
	
	for(const auto& face : cube_indices) {
		float3 edge_1 = builtin_models::cube_vertices[face.y] - builtin_models::cube_vertices[face.x];
		float3 edge_2 = builtin_models::cube_vertices[face.z] - builtin_models::cube_vertices[face.x];
		float3 norm(edge_1 ^ edge_2);
		
		builtin_models::cube_normals[face.x] += norm;
		builtin_models::cube_normals[face.y] += norm;
		builtin_models::cube_normals[face.z] += norm;
		
		// compute deltas
		float2 delta_1 = builtin_models::cube_tex_coords[face.y] - builtin_models::cube_tex_coords[face.x];
		float2 delta_2 = builtin_models::cube_tex_coords[face.z] - builtin_models::cube_tex_coords[face.x];
		
		// binormal
		float3 binormal = (edge_1 * delta_2.x) - (edge_2 * delta_1.x);
		binormal.normalize();
		
		// tangent
		float3 tangent = (edge_1 * delta_2.y) - (edge_2 * delta_1.y);
		tangent.normalize();
		
		// adjust (model specific workaround ...)
		float3 txb = tangent ^ binormal;
		(norm * txb > 0.0f) ? tangent *= -1.0f : binormal *= -1.0f;
		
		builtin_models::cube_binormals[face.x] += binormal;
		builtin_models::cube_binormals[face.y] += binormal;
		builtin_models::cube_binormals[face.z] += binormal;
		builtin_models::cube_tangents[face.x] += tangent;
		builtin_models::cube_tangents[face.y] += tangent;
		builtin_models::cube_tangents[face.z] += tangent;
	}
	
	for(auto& normal : cube_normals) {
		normal.normalize();
	}
	for(auto& binormal : cube_binormals) {
		binormal.normalize();
	}
	for(auto& tangent : cube_tangents) {
		tangent.normalize();
	}
}
