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

#ifndef __SB_GAME_BASE_H__
#define __SB_GAME_BASE_H__

#include "sb_global.h"
#include "sb_map.h"
#include <gui/gui.h>
#include <scene/scene.h>

enum class BLOCK_FACE : unsigned int;

class ai_entity;

class game_base {
public:
	virtual void set_enabled(const bool state);
	bool is_enabled() const;

	static void create_cube(GLuint& vbo, GLuint& indices_vbo);
	static void create_cube_tex_n_bn_tn(GLuint& cube_tex_vbo, GLuint& cube_normals_vbo, GLuint& cube_binormals_vbo, GLuint& cube_tangents_vbo);

protected:
	static const unsigned int invalid_pos;

	game_base();
	virtual ~game_base();

	static ray compute_scene_camera_ray();
	static ray compute_camera_scene_ray();

	static float3 compute_sceneinverse_projected_point(const float3& pos);

	struct static_intersection {
		pair<uint3, BLOCK_FACE> block_ref;
		BLOCK_MATERIAL material;
		float distance;

		bool is_invalid() const;
		const uint3& get_block_pos() const;

		static_intersection();
		static_intersection(const pair<uint3, BLOCK_FACE>& block_ref, BLOCK_MATERIAL material, float distance);
		static_intersection(const static_intersection& other);
	};
	static static_intersection intersect_static();
	static static_intersection intersect_static(const ray& sel_line);

	struct dynamic_intersection {
		rigid_body* body;
		BLOCK_MATERIAL kind;
		float distance;

		bool is_invalid() const;

		dynamic_intersection();
		dynamic_intersection(rigid_body* body, BLOCK_MATERIAL kind, float distance);
		dynamic_intersection(const dynamic_intersection& other);
	};
	static dynamic_intersection intersect_dynamic();

	struct ai_intersection {
		ai_entity* ai;
		float distance;

		bool is_invalid() const;

		ai_intersection();
		ai_intersection(const ai_intersection& other);
		ai_intersection(ai_entity* ai, float distance);
	};
	static ai_intersection intersect_ai();

	static const matrix4f compute_block_view_matrix(const uint3& pos, const float scale = 0.05f);

	bool enabled = false; // don't change this here

};

#endif
