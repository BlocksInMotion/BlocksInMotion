/*
 *  Blocks In Motion
 *  Noise: permutation and gradient tables Copyright (C) Stefan Gustavson
 *         (refer to copyright below)
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

/*  
 *  Noise permutation and gradient tables:
 *  http://staffwww.itn.liu.se/~stegu/simplexnoise/simplexnoise.pdf
 *  Copyright (C) 2004, 2005, 2010 by Stefan Gustavson. All rights reserved.
 *  This code is licensed to you under the terms of the MIT license:
 *  
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:s
  *  
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.

 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
*/

#include "game.h"
#include "save.h"
#include "block_textures.h"
#include "builtin_models.h"
#include "physics_controller.h"
#include "rigid_body.h"
#include "soft_body.h"
#include <engine.h>
#include <rendering/gfx2d.h>
#include <scene/camera.h>
#include "sb_conf.h"
#include "audio_3d.h"
#include <rendering/gl_timer.h>

static constexpr float object_grab_distance = 1.08f;
static constexpr float object_trigger_distance = 3.0f + 0.6f;
static constexpr size_t object_push_time = 150;
static constexpr size_t object_aipush_time = 150;

static constexpr size_t permutation_tex_size = 256;
static constexpr size_t permutation_tex_size_fourth = permutation_tex_size / 4;

// permutation and gradient data

// Ken Perlin's permutation and gradient tables
// http://staffwww.itn.liu.se/~stegu/simplexnoise/simplexnoise.pdf
static const unsigned char perm[permutation_tex_size] { 151,160,137,91,90,15,
	131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
	190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
	88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
	77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
	102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
	135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
	5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
	223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
	129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
	251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
	49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
	138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180 };
static const char3 grad3[16] { char3(0,1,1),char3(0,1,-1),char3(0,-1,1),char3(0,-1,-1),
	char3(1,0,1),char3(1,0,-1),char3(-1,0,1),char3(-1,0,-1),
	char3(1,1,0),char3(1,-1,0),char3(-1,1,0),char3(-1,-1,0),
	char3(1,0,-1),char3(-1,0,-1),char3(0,-1,1),char3(0,1,1) };

// Stefan Gustavson's gradient table
// http://staffwww.itn.liu.se/~stegu/simplexnoise/simplexnoise.pdf
static char4 grad4[32] = { char4(0,1,1,1), char4(0,1,1,-1), char4(0,1,-1,1), char4(0,1,-1,-1),
	char4(0,-1,1,1), char4(0,-1,1,-1), char4(0,-1,-1,1), char4(0,-1,-1,-1),
	char4(1,0,1,1), char4(1,0,1,-1), char4(1,0,-1,1), char4(1,0,-1,-1),
	char4(-1,0,1,1), char4(-1,0,1,-1), char4(-1,0,-1,1), char4(-1,0,-1,-1),
	char4(1,1,0,1), char4(1,1,0,-1), char4(1,-1,0,1), char4(1,-1,0,-1),
	char4(-1,1,0,1), char4(-1,1,0,-1), char4(-1,-1,0,1), char4(-1,-1,0,-1),
	char4(1,1,1,0), char4(1,1,-1,0), char4(1,-1,1,0), char4(1,-1,-1,0),
	char4(-1,1,1,0), char4(-1,1,-1,0), char4(-1,-1,1,0), char4(-1,-1,-1,0)};

game::game(const float3& position) :
physics_player(position),
action_handler_fct(this, &game::action_handler),
gui_callback(this, &game::draw_interface),
rendering_scene_callback(this, &game::draw_active_cube_hud) {
	old_tick = tick_push = SDL_GetTicks();
	tick_diff = 0;
	death_tex = t->add_texture(e->data_path("death_screen.png"), TEXTURE_FILTERING::LINEAR, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

	// fetch vis variables in order to speed up rendering process
	nvis_grab_color = conf::get<float4>("nvis_grab_color");
	nvis_push_color = conf::get<float4>("nvis_push_color");
	nvis_swap_color = conf::get<float4>("nvis_swap_color");
	nvis_bg_selected = conf::get<float4>("nvis_bg_selected");
	nvis_bg_selecting = conf::get<float4>("nvis_bg_selecting");
	nvis_line_interval = conf::get<float2>("nvis_line_interval");
	nvis_tex_interpolation = conf::get<float>("nvis_tex_interpolation");
	nvis_time_denominator = conf::get<float>("nvis_time_denominator");

	glGenBuffers(1, &laser_beam_vbo);

	// layout:
	//    0
	//
	// 1 ---- 2
	// 
	//   ...
	// 
	//    3

	const unsigned char laser_beam_indices[12] {
		0,1,3,
		0,2,3,
		1,2,3,
		0,1,2
	};
	glGenBuffers(1, &laser_beam_indices_vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, laser_beam_indices_vbo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 12 * sizeof(unsigned char), &laser_beam_indices[0], GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	eevt->add_internal_event_handler(action_handler_fct,
									 EVENT_TYPE::KEY_DOWN,
									 EVENT_TYPE::KEY_UP,
									 EVENT_TYPE::MOUSE_LEFT_DOWN,
									 EVENT_TYPE::MOUSE_LEFT_UP,
									 EVENT_TYPE::MOUSE_RIGHT_DOWN,
									 EVENT_TYPE::MOUSE_RIGHT_UP,
									 EVENT_TYPE::MOUSE_MIDDLE_DOWN,
									 EVENT_TYPE::MOUSE_MIDDLE_UP);

	create_cube(cube_vbo[CUBE_VBO_INDEX_VERT], cube_vbo[CUBE_VBO_INDEX_INDEX]);
	create_cube_tex_n_bn_tn(cube_vbo[CUBE_VBO_INDEX_TEX], cube_vbo[CUBE_VBO_INDEX_NORMAL],
							cube_vbo[CUBE_VBO_INDEX_BINORMAL], cube_vbo[CUBE_VBO_INDEX_TANGENT]);

	// load permutation data into tex_permutation and enforce NEAREST filtering to avoid interpolated values
	glGenTextures(1, &tex_permutation);
	glBindTexture(GL_TEXTURE_2D, tex_permutation);
	char4* permutation_data = new char4[permutation_tex_size * permutation_tex_size];
	for(size_t i = 0; i < permutation_tex_size; ++i) {
		for(size_t j = 0; j < permutation_tex_size; ++j) {
			char4* entry = permutation_data + (i * permutation_tex_size + j);
			unsigned char value = perm[(j + perm[i]) & permutation_tex_size];
			*reinterpret_cast<char3*>(entry) = (grad3[value & 16] * permutation_tex_size_fourth +
												char3(permutation_tex_size_fourth,
													  permutation_tex_size_fourth,
													  permutation_tex_size_fourth));
			entry->w = value;
		}
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, permutation_tex_size, permutation_tex_size, 0, GL_RGBA, GL_UNSIGNED_BYTE, permutation_data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	delete [] permutation_data;

	// load gradient data into tex_gradient and enforce NEAREST filtering to avoid interpolated values
	glGenTextures(1, &tex_gradient);
	glBindTexture(GL_TEXTURE_2D, tex_gradient);
	permutation_data = new char4[permutation_tex_size * permutation_tex_size];
	for(size_t i = 0; i < permutation_tex_size; ++i) {
		for(size_t j = 0; j < permutation_tex_size; ++j) {
			char4* entry = permutation_data + (i * permutation_tex_size + j);
			unsigned char value = perm[(j + perm[i]) & permutation_tex_size];
			*entry = grad4[value & 16] * permutation_tex_size_fourth + char4(permutation_tex_size_fourth,
																			 permutation_tex_size_fourth,
																			 permutation_tex_size_fourth,
																			 permutation_tex_size_fourth);
		}
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, permutation_tex_size, permutation_tex_size, 0, GL_RGBA, GL_UNSIGNED_BYTE, permutation_data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	delete [] permutation_data;
}

game::~game() {
	eevt->remove_event_handler(action_handler_fct);
	set_enabled(false);
	t->delete_texture(death_tex);
	// eleminate cubes
	for(size_t i = CUBE_VBO_INDEX_VERT; i < CUBE_VBO_INDEX_MAX__; ++i) {
		if(glIsBuffer(cube_vbo[i])) glDeleteBuffers(1, &cube_vbo[i]);
	}
	if(glIsBuffer(laser_beam_vbo)) glDeleteBuffers(1, &laser_beam_vbo);
	if(glIsBuffer(laser_beam_indices_vbo)) glDeleteBuffers(1, &laser_beam_indices_vbo);
	if(glIsTexture(tex_permutation)) glDeleteTextures(1, &tex_permutation);
	if(glIsTexture(tex_gradient)) glDeleteTextures(1, &tex_gradient);
}

void game::recompute_tick() {
	const size_t new_tick = SDL_GetTicks();
	tick_diff = new_tick - old_tick.load();
	old_tick = new_tick;
}

void game::set_enabled(const bool state) {
	if(state == enabled) return;
	game_base::set_enabled(state);
	if(enabled) {
		cb_obj = ui->add_draw_callback(DRAW_MODE_UI::PRE_UI, gui_callback, float2(1.0f), float2(0.0f));
		sce->add_draw_callback("game_hud", rendering_scene_callback);
	}
	else {
		cb_obj = nullptr;
		ui->delete_draw_callback(gui_callback);
		sce->delete_draw_callback("game_hud");
	}
}

void game::draw_crosshair(const uint2& screen_size) {
	const float crosshair_base_length = float(screen_size.x) * 0.5f;
	const float crosshair_alpha = 0.85f;
	float crosshair_length = 0.0f;
	float crosshair_angle = 15.0f;
	float4 color;
	switch(status.load()) {
		case GAME_STATUS::OBJECT_PUSHED:
			crosshair_angle = 25.0f;
			crosshair_length = 0.01f;
			color = float4(nvis_push_color.x, nvis_push_color.y, nvis_push_color.z, crosshair_alpha);
			break;
		case GAME_STATUS::OBJECT_SELECTED:
		case GAME_STATUS::OBJECT_SELECTED_FINISHED:
			crosshair_angle = 25.0f;
			crosshair_length = 0.01f;
			color = float4(nvis_grab_color.x, nvis_grab_color.y, nvis_grab_color.z, crosshair_alpha);
			break;
		default:
			crosshair_length = 0.005f;
			color = float4(1.0f, 1.0f, 1.0f, crosshair_alpha);
			break;
	}
	crosshair_length *= crosshair_base_length * 1.5f;
	const float2 half_size(screen_size / 2);
	const float offset = crosshair_length * 0.2f;
	gfx2d::draw_circle_sector_fill(half_size + float2(offset, -offset), crosshair_length, crosshair_length,
								   crosshair_angle, 90.0f - crosshair_angle,
								   color);
	gfx2d::draw_circle_sector_fill(half_size + float2(offset, offset), crosshair_length, crosshair_length,
								   90.0f + crosshair_angle, 180.0f - crosshair_angle,
								   color);
	gfx2d::draw_circle_sector_fill(half_size + float2(-offset, offset), crosshair_length, crosshair_length,
								   180.0f + crosshair_angle, 270.0f - crosshair_angle,
								   color);
	gfx2d::draw_circle_sector_fill(half_size + float2(-offset, -offset), crosshair_length, crosshair_length,
								   270.0f + crosshair_angle, 360.0f - crosshair_angle,
								   color);
}

void game::draw_center_screen_tex(const uint2& screen_size, const a2e_texture& tex, const float4& background) {
	gfx2d::draw_rectangle_fill(rect(0, 0, screen_size.x, screen_size.y), background);
			
	const uint2 tex_draw_size(tex->width, tex->height);
	const uint2 img_offset((unsigned int)screen_size.x/2 - tex_draw_size.x/2,
							(unsigned int)screen_size.y/2 - tex_draw_size.y/2);
	gfx2d::draw_rectangle_texture(rect(img_offset.x, img_offset.y,
										img_offset.x + tex_draw_size.x,
										img_offset.y + tex_draw_size.y),
										tex);
}

void game::draw_interface(const DRAW_MODE_UI draw_mode a2e_unused, rtt::fbo* buffer) {
	const uint2 screen_size(buffer->width, buffer->height);
	switch(status.load()) {
		case GAME_STATUS::DEATH:
			draw_center_screen_tex(screen_size, death_tex, float4(1.0f, 0.0f, 0.0f, 0.5f));
			break;
		case GAME_STATUS::MAP_LOAD:
			gfx2d::draw_rectangle_fill(rect(0, 0, screen_size.x, screen_size.y), float4(1.0f));
			break;
		case GAME_STATUS::AI_PUSHED:
		case GAME_STATUS::OBJECT_SWAPPED:
		case GAME_STATUS::OBJECT_PUSHED:
		case GAME_STATUS::OBJECT_SELECTED:
		case GAME_STATUS::OBJECT_SELECTED_FINISHED:
			// selected object will be drawn by the geometry-render method
		default: {
			// draw health overlay
			const float cur_health = get_health();
			if(cur_health < 100.0f) {
				const float4 health_background(1.0f, 0.0f, 0.0f, (1.0f - (get_health() / 100.0f)));
				gfx2d::draw_rectangle_fill(rect(0, 0, screen_size.x, screen_size.y), health_background);
			}
			
			// draw crosshair
			draw_crosshair(screen_size);
		}
		break;
	}
}

void game::physics_update() {
	// handle active object
	switch(status.load()) {
		case GAME_STATUS::OBJECT_SELECTED: {
			// grab object with growing force
			float3 target_dir = get_block_target_pos() - body->get_position();
			float len = target_dir.length();
			if(len < object_grab_distance) {
				set_status(GAME_STATUS::OBJECT_SELECTED_FINISHED);
				// hide object and remove it from physics engine
				active_map->update_dynamic(body, BLOCK_MATERIAL::NONE);
				// unregister physical object
				pc->disable_body(body);
			}
			else {
				// speed up body -> min scale 3
				target_dir = target_dir.normalize() * std::max<float>(len, 0.5f);
				body->get_body()->setLinearVelocity(btVector3(target_dir[0], target_dir[1], target_dir[2]));
			}
		} break;
		case GAME_STATUS::OBJECT_SELECTED_FINISHED: {
			// nothing to do
			break;
		}
		default: break;
	}
	
	static float old_health = -1.0f;
	const float cur_health = get_health();
	if(cur_health != old_health && cur_health < 100.0f && cb_obj != nullptr) {
		old_health = cur_health;
		cb_obj->redraw();
	}
	physics_player::physics_update();
}

void game::draw_selection_tracking(const float3& block_pos,
								   const float4& selection_color, const float3& target_pos) {
	gl3shader shd = s->get_gl3shader("SELECTED_BLOCK");
	const float ray_dim = 0.3f;

	// use colored version
	shd->use("colored");
	
	shd->uniform("time", float(SDL_GetTicks()) / nvis_time_denominator);
	shd->uniform("model_view_projection_matrix", *e->get_translation_matrix() *
				 *e->get_rotation_matrix() * *e->get_projection_matrix());
	shd->uniform("forcefield_foreground", selection_color);
	shd->uniform("forcefield_background", nvis_bg_selecting);
	shd->uniform("forcefield_line_int", nvis_line_interval);

	const float3 local_down_vec = compute_sceneinverse_projected_point(float3(0.0f, -1.0, 0.0f)).normalize() * ray_dim;
	
	glBindBuffer(GL_ARRAY_BUFFER, laser_beam_vbo);

	const float3 block_pos_res = block_pos + compute_sceneinverse_projected_point(float3(0.0f, 1.0f, 0.0f)).normalize() * ray_dim;

	const float3 line_points[4] =
	{
		block_pos_res,
		block_pos_res + local_down_vec + compute_sceneinverse_projected_point(float3(-1.0f, 0.0f, 0.0f)).normalize() * ray_dim,
		block_pos_res + local_down_vec + compute_sceneinverse_projected_point(float3(1.0f, 0.0f, 0.0f)).normalize() * ray_dim,
		target_pos
	};

	glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(float3), &line_points[0], GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// data arrays
	shd->attribute_array("in_vertex", laser_beam_vbo, 3);

	// set textures
	shd->texture("noise_permutation", tex_permutation, GL_TEXTURE_2D);
	shd->texture("noise_gradient", tex_gradient, GL_TEXTURE_2D);
	
	// draw
	glDisable(GL_CULL_FACE);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, laser_beam_indices_vbo);
	glDrawElements(GL_TRIANGLES, 12 * sizeof(unsigned char), GL_UNSIGNED_BYTE, nullptr);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glEnable(GL_CULL_FACE);
	
	// disable everything
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	shd->disable();
}

void game::draw_selection_cube(const float3& block_pos, const matrix4f& rot_matrix,
							   const float4& selection_color, const float3& target_pos a2e_unused) {
	// use simple alpha-colored shader for selection
	gl3shader shd = s->get_gl3shader("SELECTED_BLOCK");
	const float cube_outline_scale = 0.06f;
	const float cube_scale = 1.0f + cube_outline_scale;

	matrix4f mvm = matrix4f().scale(cube_scale, cube_scale, cube_scale);
	mvm *= matrix4f().translate(-cube_scale * 0.5f, -cube_scale * 0.5f, -cube_scale * 0.5f);
	mvm *= rot_matrix;
	mvm *= *e->get_translation_matrix();
	mvm *= matrix4f().translate(block_pos.x, block_pos.y, block_pos.z);
	mvm *= *e->get_rotation_matrix();

	// use colored variant
	shd->use("colored");
	
	shd->uniform("time", float(SDL_GetTicks()) / (nvis_time_denominator * 1000.0f));
	shd->uniform("model_view_projection_matrix", mvm * *e->get_projection_matrix());
	shd->uniform("forcefield_foreground", selection_color);
	shd->uniform("forcefield_background", nvis_bg_selecting);
	shd->uniform("forcefield_line_int", nvis_line_interval);
	
	// points
	shd->attribute_array("in_vertex", cube_vbo[CUBE_VBO_INDEX_VERT], 3);
	
	// draw
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube_vbo[CUBE_VBO_INDEX_INDEX]);
	glDrawElements(GL_TRIANGLES, (GLsizei)builtin_models::cube_index_count * 3, GL_UNSIGNED_BYTE, nullptr);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		
	// disable everything
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	shd->disable();
}

void game::draw_fp_cube(const matrix4f& mod_matrix, const float3& target_pos) {
	gl3shader shd = s->get_gl3shader("SELECTED_BLOCK");

	constexpr float cube_scale = 0.7f;

	matrix4f mm = matrix4f().scale(cube_scale, cube_scale, cube_scale);
	mm *= matrix4f().translate(-cube_scale * 0.5f, -cube_scale * 0.5f, -cube_scale * 0.5f);
	mm *= mod_matrix;
	mm *= *e->get_translation_matrix();
	mm *= matrix4f().translate(target_pos[0], target_pos[1], target_pos[2]);

	// use textured version
	shd->use("textured");
	
	shd->uniform("time", float(SDL_GetTicks()) / nvis_time_denominator);
	shd->uniform("block_mat", (float)selected_block_mat);
	shd->uniform("model_matrix", mm);
	shd->uniform("camera_position", -float3(*e->get_position()));
	shd->uniform("model_view_projection_matrix", mm * *e->get_rotation_matrix() * *e->get_projection_matrix());
	shd->uniform("forcefield_foreground", nvis_grab_color);
	shd->uniform("forcefield_background", nvis_bg_selected);
	shd->uniform("forcefield_line_int", nvis_line_interval);
	shd->uniform("forcefield_color_inter", nvis_tex_interpolation);

	// data arrays
	shd->attribute_array("in_vertex", cube_vbo[CUBE_VBO_INDEX_VERT], 3);
	shd->attribute_array("texture_coord", cube_vbo[CUBE_VBO_INDEX_TEX], 2);
	shd->attribute_array("normal", cube_vbo[CUBE_VBO_INDEX_NORMAL], 3);
	shd->attribute_array("binormal", cube_vbo[CUBE_VBO_INDEX_BINORMAL], 3);
	shd->attribute_array("tangent", cube_vbo[CUBE_VBO_INDEX_TANGENT], 3);

	// set textures
	shd->texture("noise_permutation", tex_permutation, GL_TEXTURE_2D);
	shd->texture("noise_gradient", tex_gradient, GL_TEXTURE_2D);
	shd->texture("diffuse_textures", bt->tex_id(0), GL_TEXTURE_2D_ARRAY);
	shd->texture("height_textures", bt->tex_id(4), GL_TEXTURE_2D_ARRAY);

	// draw
	glDisable(GL_DEPTH_TEST);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube_vbo[CUBE_VBO_INDEX_INDEX]);
	glDrawElements(GL_TRIANGLES, (GLsizei)builtin_models::cube_index_count * 3, GL_UNSIGNED_BYTE, nullptr);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glEnable(GL_DEPTH_TEST);
		
	// disable everything
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	shd->disable();
}

void game::reset_status_in_time(const size_t& base_time) {
	// reset status since we have successfully drawn a selection once
	if(old_tick.load() - tick_push.load() > base_time) {
		set_status(GAME_STATUS::IDLE);
	}
}

void game::draw_active_cube_hud(const DRAW_MODE draw_mode) {
	recompute_tick();
	if(draw_mode != DRAW_MODE::MATERIAL_ALPHA_PASS || body == nullptr) return;
	
	gl_timer::mark("HUD_START");
	const float3 target_pos(get_block_target_pos());
	const matrix4f& rot_matrix = body->get_rotation();
	const float3 block_pos(body->get_position());
	switch(status.load()) {
		case GAME_STATUS::AI_PUSHED: {
			draw_selection_tracking(block_pos, nvis_push_color, target_pos);
			reset_status_in_time(object_aipush_time);
			break;
		}
		case GAME_STATUS::OBJECT_PUSHED:
		case GAME_STATUS::OBJECT_SWAPPED: {
			const float4 color = status.load() == GAME_STATUS::OBJECT_PUSHED ? nvis_push_color : nvis_swap_color;
			draw_selection_tracking(block_pos, color, target_pos);
			draw_selection_cube(block_pos, rot_matrix, color, target_pos);
			reset_status_in_time(object_push_time);
			break;
		}
		case GAME_STATUS::OBJECT_SELECTED: {
			// mark selection and draw camera-object beam
			draw_selection_tracking(block_pos, nvis_grab_color, target_pos);
			draw_selection_cube(block_pos, rot_matrix, nvis_grab_color, target_pos);
			break;
		}
		case GAME_STATUS::OBJECT_SELECTED_FINISHED: {
			// draw rotating cube -> use largin scaling factor in order to reduce the rotation speed
			const float rot_angle = sinf(float(SDL_GetTicks()) / 2500.0f);
			const matrix4f cube_rot_mat = matrix4f().rotate_y(360.0f * rot_angle);
			draw_fp_cube(cube_rot_mat, target_pos);
			break;
		}
		default:
			// nothing to draw here
			break;
	}
	gl_timer::mark("HUD_END");
}

bool game::handle_object(const WEAPON_MODE& weapon_mode) {
	// try to find a static block first
	const game_base::static_intersection static_int(game_base::intersect_static());
	const game_base::dynamic_intersection dynamic_int(game_base::intersect_dynamic());
	const game_base::ai_intersection ai_int(game_base::intersect_ai());
	body = nullptr;
	float dist = static_int.distance;
	if(!static_int.is_invalid() && dist < dynamic_int.distance) {
		// take static match
		if(static_int.material == BLOCK_MATERIAL::METAL) {
			body = active_map->resolve_dynamic(static_int.get_block_pos());
			selected_block_mat = static_int.material;
		}
	}
	else if(dynamic_int.kind != BLOCK_MATERIAL::NONE) {
		// take dynamic block
		body = dynamic_int.body;
		selected_block_mat = dynamic_int.kind;
		dist = dynamic_int.distance;
	}

	if(!ai_int.is_invalid() && (body == nullptr || dist > ai_int.distance)) {
		// extra push handling for AI
		if(weapon_mode != WEAPON_MODE::FORCE) return false;
		body = ai_int.ai->get_character_body();
		ai_int.ai->force_push_ai();
		// store time stamp for limitted time tractor beam
		tick_push = old_tick.load();
		set_status(GAME_STATUS::AI_PUSHED);
		
		ai_int.ai->force_push_ai();
		return true;
	}
	else if(body == nullptr) {
		// nothing found
		return false;
	}

	switch(weapon_mode) {
		case WEAPON_MODE::ATTRACT:
			// we have successfully grabed an object
			pc->lock();
			body->get_body()->setGravity(btVector3(0.0f, 0.0f, 0.0f));
			pc->unlock();
			set_status(GAME_STATUS::OBJECT_SELECTED);
			break;
		case WEAPON_MODE::SWAP: {
			pc->lock();
			btRigidBody* realBody = body->get_body();
			float grav = realBody->getGravity().y();
			if(fabsf(grav) < 0.1f) grav = -1.0f;
			realBody->setGravity(btVector3(0.0f, -grav, 0.0f));
			pc->unlock();
			// swap state and play sound
			set_status(GAME_STATUS::OBJECT_SWAPPED);
			
			// store time stamp for limitted time tractor beam
			tick_push = old_tick.load();
			break;
		}
		case WEAPON_MODE::FORCE:
			// accelerate object -> apply velocity in camera direction
			pc->lock();
			body->get_body()->setLinearVelocity(get_object_target_dirvec() * 1.5f);
			pc->unlock();
			set_status(GAME_STATUS::OBJECT_PUSHED);
			// store time stamp
			tick_push = old_tick.load();
			break;
	}

	return true;
}

bool game::release_object(const WEAPON_MODE& weapon_mode) {
	if(body == nullptr) return true;
	
	pc->lock();
	
	btRigidBody* realBody = body->get_body();
	if(status == GAME_STATUS::OBJECT_SELECTED_FINISHED) {
		// set new position in front of the player
		body->set_position(get_block_target_spawn_pos());
		// register object
		pc->enable_body(body);
		// make object visible
		active_map->update_dynamic(body, selected_block_mat);
	}
	switch(weapon_mode) {
		case WEAPON_MODE::FORCE: {
			// force object to stop and add additional power
			active_map->play_sound("TRACTORBEAM_START", get_position(), 1.0f);

			float vel_length = realBody->getLinearVelocity().absolute().length();
			realBody->setLinearVelocity(get_object_target_dirvec() * std::min(vel_length, 1.5f));
			realBody->setGravity(physics_controller::get_global_bullet_gravity());
			break;
		}
		case WEAPON_MODE::ATTRACT:
			// drop body -> small force to simulate drop
			if (current_tractorbeam_loop_id != "") {
				active_map->stop_sound(current_tractorbeam_loop_id);
			}
			active_map->play_sound("TRACTORBEAM_END", get_position(), 1.0f);

			realBody->setLinearVelocity(get_object_target_dirvec(2.0f));
			realBody->setGravity(physics_controller::get_global_bullet_gravity());
			break;
		case WEAPON_MODE::SWAP:
			// do nothing
			break;
	}
	pc->unlock();
	set_status(GAME_STATUS::IDLE);
	return true;
}

bool game::action_handler(EVENT_TYPE type, shared_ptr<event_object> obj) {
	if(!enabled || active_map == nullptr) return false;
	
	const pair<controls::ACTION, bool> action { controls::action_from_event(type, obj) };
	switch(action.first) {
		case controls::ACTION::PHYSICSGUN_PUSH:
			if(action.second) {
				// grab object
				switch(status.load()) {
					case GAME_STATUS::IDLE:
						return handle_object(WEAPON_MODE::FORCE);
					case GAME_STATUS::OBJECT_SELECTED:
						if(release_object(WEAPON_MODE::FORCE)) {
							set_status(GAME_STATUS::OBJECT_PUSHED);
							return true;
						}
						else break;
					case GAME_STATUS::OBJECT_SELECTED_FINISHED:
						return release_object(WEAPON_MODE::FORCE);
					default: break;
				}
			}
			else {
				switch(status.load()) {
					case GAME_STATUS::OBJECT_PUSHED:
						set_status(GAME_STATUS::IDLE);
#if defined(__clang__)
						[[clang::fallthrough]];
#endif
					case GAME_STATUS::OBJECT_SELECTED:
					case GAME_STATUS::OBJECT_SELECTED_FINISHED:
						return release_object(WEAPON_MODE::FORCE);
					default: break;
				}
			}
			break;
		case controls::ACTION::PHYSICSGUN_PULL:
			if(action.second) {
				switch(status.load()) {
					case GAME_STATUS::IDLE: {
						const game_base::static_intersection clicked_block(game_base::intersect_static());
						if(!clicked_block.is_invalid() && clicked_block.distance < object_trigger_distance) {
							bool clicked = false;
							active_map->handle_block_click(clicked_block.block_ref, clicked);
							if(clicked) return true;
						}
						return handle_object(WEAPON_MODE::ATTRACT);
					}
					default: break;
				}
			}
			else {
				switch(status.load()) {
					case GAME_STATUS::OBJECT_SELECTED:
					case GAME_STATUS::OBJECT_SELECTED_FINISHED:
						return release_object(WEAPON_MODE::ATTRACT);
					default: break;
				}
			}
			break;
		case controls::ACTION::PHYSICSGUN_GRAVITY:
			if(action.second) {
				switch(status.load()) {
					case GAME_STATUS::IDLE:
						return handle_object(WEAPON_MODE::SWAP);
					default: break;
				}
			}
			break;
		default: return false;
	}
	
	return false;
}

btVector3 game::get_object_target_dirvec(const float scale) const {
	float3 val = game_base::compute_camera_scene_ray().direction * scale;
	return btVector3(val[0], val[1], val[2]);
}

float3 game::get_block_target_pos() {
	// take original pos instead of a modified one to provider a better user experience
	return -cam->get_position() + compute_sceneinverse_projected_point(float3(0.0f, -1.1f, 0.0f));
}

float3 game::get_block_target_spawn_pos() {
	// +1z (own distance) + 1z (block size) + 0.8z (offset)
	return -cam->get_position() + compute_sceneinverse_projected_point(float3(0.0f, -0.2f, 2.2f));
}

GAME_STATUS game::set_status(const GAME_STATUS& status_) {
	const GAME_STATUS old = status.load();
	status = status_;
	if(status != old && cb_obj != nullptr) {
		cb_obj->redraw();
	}
	
	switch(status.load()) {
		case GAME_STATUS::AI_PUSHED:
		case GAME_STATUS::OBJECT_SWAPPED:
		case GAME_STATUS::OBJECT_PUSHED:
			active_map->play_sound("TRACTORBEAM_START", get_position(), 1.0f);
			break;

		case GAME_STATUS::OBJECT_SELECTED:
			current_tractorbeam_loop_id = active_map->play_sound("TRACTORBEAM_LOOP", get_position(),
																 1.0f, true)->get_identifier();
			break;

		case GAME_STATUS::OBJECT_SELECTED_FINISHED:
			if (current_tractorbeam_loop_id != "") {
				active_map->stop_sound(current_tractorbeam_loop_id);
			}
			active_map->play_sound("TRACTORBEAM_FINISHED", get_position(), 1.0f);
			break;
		case GAME_STATUS::DEATH:
			save->inc_death_count();
#if defined(__clang__)
			[[clang::fallthrough]];
#endif
		case GAME_STATUS::MAP_LOAD:
			body = nullptr;
			break;

		default:
			break;
	}
	return old;
}

GAME_STATUS game::get_status() const {
	return status;
}

void game::damage(const float& value) {
	set_health(get_health() - value);
	set_timer(SDL_GetTicks());

	if(cb_obj != nullptr) {
		cb_obj->redraw();
	}
	
	if (get_health() <= 0.0f) {
		reset_parameters();

		set_status(GAME_STATUS::DEATH);
		active_map->play_sound("WILHELM_SCREAM", get_position(), 1.0f, false, false);
		active_map->kill_player();
	}
	else {
		active_map->play_sound("JAB", get_position());
	}
}

bool game::is_in_line_of_sight(const float3& pos, const float& max_distance) const {
	const static_intersection intersection = intersect_static(ray(pos, (get_position() - pos).normalized()));
	return intersection.is_invalid() || (intersection.distance > max_distance);
}
