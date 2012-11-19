/*
 *  Blocks In Motion
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

#include "game_base.h"
#include "block_textures.h"
#include "builtin_models.h"
#include "rigid_body.h"
#include "ai_entity.h"
#include <engine.h>

const unsigned int game_base::invalid_pos = (~0u);

game_base::game_base() {
}

bool game_base::is_enabled() const {
	return enabled;
}

void game_base::set_enabled(const bool state) {
	enabled = state;
}

game_base::~game_base() {
}

void game_base::create_cube(GLuint& cube_vbo, GLuint& cube_indices_vbo) {
	glGenBuffers(1, &cube_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, cube_vbo);
	glBufferData(GL_ARRAY_BUFFER, builtin_models::cube_vertex_count * sizeof(float3), &builtin_models::cube_vertices[0], GL_STATIC_DRAW);
	
	glGenBuffers(1, &cube_indices_vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube_indices_vbo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, builtin_models::cube_index_count * sizeof(uchar3), &builtin_models::cube_indices[0], GL_STATIC_DRAW);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void game_base::create_cube_tex_n_bn_tn(GLuint& cube_tex_vbo, GLuint& cube_normals_vbo, GLuint& cube_binormals_vbo, GLuint& cube_tangents_vbo) {
	glGenBuffers(1, &cube_tex_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, cube_tex_vbo);
	glBufferData(GL_ARRAY_BUFFER, builtin_models::cube_vertex_count * sizeof(coord), &builtin_models::cube_tex_coords[0], GL_STATIC_DRAW);
	
	glGenBuffers(1, &cube_normals_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, cube_normals_vbo);
	glBufferData(GL_ARRAY_BUFFER, builtin_models::cube_vertex_count * sizeof(float3), &builtin_models::cube_normals[0], GL_STATIC_DRAW);
	
	glGenBuffers(1, &cube_binormals_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, cube_binormals_vbo);
	glBufferData(GL_ARRAY_BUFFER, builtin_models::cube_vertex_count * sizeof(float3), &builtin_models::cube_binormals[0], GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	glGenBuffers(1, &cube_tangents_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, cube_tangents_vbo);
	glBufferData(GL_ARRAY_BUFFER, builtin_models::cube_vertex_count * sizeof(float3), &builtin_models::cube_tangents[0], GL_STATIC_DRAW);
		
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

const matrix4f game_base::compute_block_view_matrix(const uint3& pos, const float scale) {
	matrix4f result = matrix4f().scale(1.0f+scale, 1.0f+scale, 1.0f+scale);
	result *= *e->get_translation_matrix();
	result *= matrix4f().translate(pos.x - scale*0.5f,
								   pos.y - scale*0.5f,
								   pos.z - scale*0.5f);
	result *= *e->get_rotation_matrix();
	return result;
}

game_base::dynamic_intersection game_base::intersect_dynamic() {
	dynamic_intersection closest_block = { nullptr, BLOCK_MATERIAL::NONE, numeric_limits<float>::max() };
	if(active_map == nullptr) return closest_block;
	ray sel_line = compute_camera_scene_ray();
	pair<float, float> ret;

	for(const auto& dyn_body : active_map->get_dynamic_bodies()) {
		bbox box;
		dyn_body.first->compute_bbox(box);
		box.intersect(ret, sel_line);
		if(ret.first < ret.second && ret.first < closest_block.distance) {
			// store closest hit
			closest_block = dynamic_intersection(dyn_body.first, dyn_body.second, ret.first);
		}
	}
	return closest_block;
}

ray game_base::compute_scene_camera_ray() {
	ray camera_scene = compute_camera_scene_ray();
	return ray(camera_scene.origin, -camera_scene.direction);
}

float3 game_base::compute_sceneinverse_projected_point(const float3& pos) {
	const matrix4d view_matrix(*e->get_rotation_matrix());
	const matrix4d proj_matrix(matrix4d().perspective((double)e->get_fov(), double(e->get_width()) / double(e->get_height()),
													  (double)e->get_near_far_plane().x, (double)e->get_near_far_plane().y));
	return pos * (view_matrix * proj_matrix).invert();
}

ray game_base::compute_camera_scene_ray() {
	const float3 proj_pos = compute_sceneinverse_projected_point(float3(0.0f, 0.0f, 1.0f));
	// create ray from camera to this point
	return ray(-*e->get_position(), proj_pos.normalized());
}

game_base::static_intersection::static_intersection(const static_intersection& other)
	: block_ref(other.block_ref)
	, material(other.material)
	, distance(other.distance) {
}

game_base::static_intersection::static_intersection()
	: block_ref(uint3(invalid_pos), BLOCK_FACE::INVALID)
	, material(BLOCK_MATERIAL::NONE)
	, distance(numeric_limits<float>::max()) {
}

game_base::static_intersection::static_intersection(const pair<uint3, BLOCK_FACE>& block_ref_, BLOCK_MATERIAL material_, float distance_)
	: block_ref(block_ref_)
	, material(material_)
	, distance(distance_) {
}

const uint3& game_base::static_intersection::get_block_pos() const {
	return block_ref.first;
}

bool game_base::static_intersection::is_invalid() const {
	return (get_block_pos().x == invalid_pos);
}

game_base::dynamic_intersection::dynamic_intersection(const dynamic_intersection& other)
	: body(other.body)
	, kind(other.kind)
	, distance(other.distance) {
}

game_base::dynamic_intersection::dynamic_intersection()
	: body(nullptr)
	, kind(BLOCK_MATERIAL::NONE)
	, distance(numeric_limits<float>::max()) {
}

game_base::dynamic_intersection::dynamic_intersection(rigid_body* body_, BLOCK_MATERIAL kind_, float distance_)
	: body(body_)
	, kind(kind_)
	, distance(distance_) {
}

bool game_base::dynamic_intersection::is_invalid() const {
	return (body == nullptr);
}

game_base::static_intersection game_base::intersect_static() {
	return intersect_static(compute_camera_scene_ray());
}

game_base::static_intersection game_base::intersect_static(const ray& sel_line) {
	if(active_map == nullptr) return static_intersection();
	
	// first: intersect all chunk boxes (so we know which to check and in what order)
	vector<pair<float, unsigned int>> intersected_chunks;
	pair<float, float> ret;
	for(unsigned int chunk_index = 0, chunk_max_index = (unsigned int)active_map->get_chunks().size();
		chunk_index < chunk_max_index; chunk_index++) {
		const uint3 pos = active_map->chunk_index_to_position(chunk_index) * sb_map::chunk_extent;
		const bbox chunk_bbox(pos, pos + uint3(sb_map::chunk_extent));
		chunk_bbox.intersect(ret, sel_line);
		if(ret.first < ret.second) {
			intersected_chunks.emplace_back(ret.first, chunk_index);
		}
	}
	if(intersected_chunks.empty()) return static_intersection();
	
	// sort by intersection distance
	sort(begin(intersected_chunks), end(intersected_chunks),
		 [](const decltype(intersected_chunks)::value_type& elem_0,
			const decltype(intersected_chunks)::value_type& elem_1) {
			 return (elem_0.first < elem_1.first);
	});
	
	// second: intersect all blocks within a chunk (starting with the closest chunk)
	pair<float, unsigned int> closest_block = { numeric_limits<float>::max(), 0 };
	uint3 global_position(0, 0, 0);
	for(const auto& chunk : intersected_chunks) {
		global_position.set(active_map->chunk_index_to_position(chunk.second) * sb_map::chunk_extent);
		closest_block = { numeric_limits<float>::max(), 0 };
		vector<pair<float, unsigned int>> intersected_blocks;
		for(unsigned int block_index = 0; block_index < sb_map::blocks_per_chunk; block_index++) {
			if(active_map->get_block(chunk.second, block_index).material != BLOCK_MATERIAL::NONE) {
				const uint3 pos = sb_map::block_index_to_position(block_index) + global_position;
				const bbox block_bbox(pos, pos + uint3(1, 1, 1));
				block_bbox.intersect(ret, sel_line);
				if(ret.first < ret.second && ret.first >= 0.0f) {
					intersected_blocks.emplace_back(ret.first, block_index);
				}
			}
		}
		if(!intersected_blocks.empty()) {
			// we have a block intersection, find the closest one and break (since we know it's the closest one)
			for_each(begin(intersected_blocks), end(intersected_blocks),
					 [&closest_block](const decltype(intersected_blocks)::value_type& elem) {
						 if(elem.first < closest_block.first) {
							 closest_block = elem;
						 }
			});
			break;
		}
	}
	
	static const float max_distance = sb_map::chunk_extent * 8; // 128.0f
	if(closest_block.first > max_distance) return static_intersection();
	
	// check which block face is selected
	const uint3 local_position = sb_map::block_index_to_position(closest_block.second);
	const bbox intersection_bbox(local_position + global_position, local_position + global_position + uint3(1, 1, 1));
	BLOCK_FACE face = BLOCK_FACE::INVALID;
	const float3 intersection_pos = sel_line.origin + sel_line.direction * (closest_block.first); // add a small epsilon
	if(FLOAT_EQ(intersection_pos.x, intersection_bbox.min.x)) face = BLOCK_FACE::LEFT;
	else if(FLOAT_EQ(intersection_pos.x, intersection_bbox.max.x)) face = BLOCK_FACE::RIGHT;
	else if(FLOAT_EQ(intersection_pos.y, intersection_bbox.min.y)) face = BLOCK_FACE::BOTTOM;
	else if(FLOAT_EQ(intersection_pos.y, intersection_bbox.max.y)) face = BLOCK_FACE::TOP;
	else if(FLOAT_EQ(intersection_pos.z, intersection_bbox.min.z)) face = BLOCK_FACE::FRONT;
	else if(FLOAT_EQ(intersection_pos.z, intersection_bbox.max.z)) face = BLOCK_FACE::BACK;
	
	const uint3 block_ref_pos(local_position + global_position);
	const sb_map::block_data& block_ref_data = active_map->get_block(block_ref_pos);
	return static_intersection(make_pair(block_ref_pos, face), block_ref_data.material, closest_block.first);
}

game_base::ai_intersection::ai_intersection(const ai_intersection& other)
	: ai(other.ai)
	, distance(other.distance) {
}

game_base::ai_intersection::ai_intersection()
	: ai(nullptr)
	, distance(numeric_limits<float>::max()) {
}

game_base::ai_intersection::ai_intersection(ai_entity* ai_, float distance_)
	: ai(ai_)
	, distance(distance_) {
}

bool game_base::ai_intersection::is_invalid() const {
	return (ai == nullptr);
}

game_base::ai_intersection game_base::intersect_ai() {
	ai_intersection closest_ai;
	if(active_map == nullptr) return closest_ai;
	ray sel_line = compute_camera_scene_ray();
	pair<float, float> ret;
	for(const auto& ai : active_map->get_ai_entities()) {
		rigid_body* ai_body = ai->get_character_body();
		bbox box;
		ai_body->compute_bbox(box);
		box.intersect(ret, sel_line);
		if(ret.first < ret.second && ret.first < closest_ai.distance) {
			// store closest hit
			closest_ai = ai_intersection(ai, ret.first);
		}
	}
	return closest_ai;
}
