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

#ifndef __SB_GAME_H__
#define __SB_GAME_H__

#include "sb_global.h"
#include "sb_map.h"
#include "physics_player.h"
#include "ai_entity.h"
#include "game_base.h"
#include <gui/gui.h>
#include <scene/scene.h>

enum class GAME_STATUS {
	IDLE,
	AI_PUSHED,
	OBJECT_PUSHED,
	OBJECT_SWAPPED,
	OBJECT_SELECTED,
	OBJECT_SELECTED_FINISHED,
	DEATH,
	MAP_LOAD,
};

class game : public game_base, public physics_player {
public:
	game(const float3& position);
	virtual ~game();
	
	virtual void physics_update();
	virtual void set_enabled(const bool state);
	
	GAME_STATUS set_status(const GAME_STATUS& status);
	GAME_STATUS get_status() const;
	
	virtual void damage(const float& value);
	bool is_in_line_of_sight(const float3& pos, const float& max_distance) const;
	
protected:
	enum class WEAPON_MODE {
		FORCE,
		ATTRACT,
		SWAP,
	};
	
	void draw_crosshair(const uint2& screen_size);
	
	void reset_status_in_time(const size_t& base_time);
	
	bool handle_object(const WEAPON_MODE& weapon_mode);
	bool release_object(const WEAPON_MODE& weapon_mode);
	
	btVector3 get_object_target_dirvec(const float scale = 7.0f) const;
	
	static float3 get_block_target_pos();
	static float3 get_block_target_spawn_pos();
	static float4 get_pos_color();
	
	atomic<size_t> old_tick { 0 }, tick_diff { 0 }, tick_push { 0 };
	
	atomic<GAME_STATUS> status { GAME_STATUS::IDLE };
	unsigned int health = 100;
	
	BLOCK_MATERIAL selected_block_mat = BLOCK_MATERIAL::NONE;
	rigid_body* body = nullptr;
	ai_entity* ai = nullptr;
	
	void recompute_tick();
	
	event::handler action_handler_fct;
	bool action_handler(EVENT_TYPE type, shared_ptr<event_object> obj);
	
	// gui rendering
	ui_draw_callback gui_callback;
	gui_simple_callback* cb_obj = nullptr;
	void draw_interface(const DRAW_MODE_UI draw_mode, rtt::fbo* buffer);
	
	void draw_center_screen_tex(const uint2& screen_size,
								const a2e_texture& tex,
								const float4& background);
	
	a2e_texture death_tex;
	
	// rendering geometry
	scene::draw_callback rendering_scene_callback;
	void draw_active_cube_hud(const DRAW_MODE draw_mode);
	
	void draw_selection_tracking(const float3& pos,
								 const float4& selection_color,
								 const float3& target_pos);
	
	void draw_selection_cube(const float3& pos,
							 const matrix4f& rot_matrix,
							 const float4& selection_color,
							 const float3& target_pos);
	
	void draw_fp_cube(const matrix4f& mod_matrix,
					  const float3& target_pos);
	
	enum CUBE_VBO_INDEX {
		CUBE_VBO_INDEX_VERT = 0,
		CUBE_VBO_INDEX_INDEX = 1,
		CUBE_VBO_INDEX_TEX = 2,
		CUBE_VBO_INDEX_NORMAL = 3,
		CUBE_VBO_INDEX_BINORMAL = 4,
		CUBE_VBO_INDEX_TANGENT = 5,
		CUBE_VBO_INDEX_MAX__
	};
	
	GLuint tex_permutation = 0, tex_gradient = 0;
	array<GLuint, 6> cube_vbo { { 0, 0, 0, 0, 0, 0} };
	GLuint laser_beam_vbo = 0, laser_beam_indices_vbo = 0;
	
	// noise visualization settings
	float4 nvis_grab_color, nvis_push_color, nvis_swap_color;
	float4 nvis_bg_selected, nvis_bg_selecting;
	float2 nvis_line_interval;
	float nvis_time_denominator, nvis_tex_interpolation;
	
	string current_tractorbeam_loop_id = "";
};

#endif
