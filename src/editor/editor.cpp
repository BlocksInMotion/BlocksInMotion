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

#include "editor.h"
#include "block_textures.h"
#include "builtin_models.h"
#include "audio_3d.h"
#include "audio_background.h"
#include "physics_controller.h"
#include "rigid_body.h"
#include "soft_body.h"
#include "game.h"
#include "script_handler.h"
#include "audio_controller.h"
#include "sb_console.h"
#include <engine.h>
#include <rendering/gfx2d.h>
#include <scene/camera.h>
#include <gui/style/gui_theme.h>
#include <gui/objects/gui_window.h>

constexpr size_t editor::object_count;

editor::editor() :
ui_theme(ui->get_theme()),
draw_cb(this, &editor::draw_ui),
event_handler_fnctr(this, &editor::event_handler),
scene_draw_cb(this, &editor::draw_selection),
selected_block(uint3(invalid_pos), BLOCK_FACE::INVALID),
block_selection(selected_block, selected_block)
{
	create_cube(cube_vbo, cube_indices_vbo);
	
	// unfortunately, these must be loaded twice due to filtering and mip-mapping issues
	object_textures = {
		{
			t->add_texture(e->data_path("icons/sound.png"), TEXTURE_FILTERING::TRILINEAR, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE),
			t->add_texture(e->data_path("icons/link.png"), TEXTURE_FILTERING::TRILINEAR, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE),
			t->add_texture(e->data_path("icons/trigger.png"), TEXTURE_FILTERING::TRILINEAR, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE),
			t->add_texture(e->data_path("icons/light.png"), TEXTURE_FILTERING::TRILINEAR, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE),
			t->add_texture(e->data_path("icons/ai_waypoint.png"), TEXTURE_FILTERING::TRILINEAR, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE),
			t->add_texture(e->data_path("icons/player.png"), TEXTURE_FILTERING::TRILINEAR, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE),
		}
	};
	static constexpr size_t icon_count = object_count+1;
	array<a2e_texture, icon_count> object_textures_3d {
		{
			t->add_texture(e->data_path("icons/sound.png"), TEXTURE_FILTERING::LINEAR, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE),
			t->add_texture(e->data_path("icons/link.png"), TEXTURE_FILTERING::LINEAR, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE),
			t->add_texture(e->data_path("icons/trigger.png"), TEXTURE_FILTERING::LINEAR, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE),
			t->add_texture(e->data_path("icons/light.png"), TEXTURE_FILTERING::LINEAR, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE),
			t->add_texture(e->data_path("icons/ai_waypoint.png"), TEXTURE_FILTERING::LINEAR, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE),
			t->add_texture(e->data_path("icons/player.png"), TEXTURE_FILTERING::LINEAR, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE),
		}
	};
	
	// pre-render object textures
	static a2e_constexpr size2 object_texture_size(512);
	rtt* r = e->get_rtt();
	rtt::fbo* buffer = r->add_buffer(object_texture_size.x, object_texture_size.y, GL_TEXTURE_2D,
									 TEXTURE_FILTERING::POINT, e->get_ui_anti_aliasing(),
									 GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 1,
									 rtt::DEPTH_TYPE::RENDERBUFFER);
	
	glGenTextures(1, &object_texture_array);
	glBindTexture(GL_TEXTURE_2D_ARRAY, object_texture_array);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, object_texture_size.x, object_texture_size.y,
				 (GLsizei)icon_count*2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	
	//
	glEnable(GL_BLEND);
	glEnable(GL_SCISSOR_TEST);
	glDepthFunc(GL_LEQUAL);
	gfx2d::set_blend_mode(gfx2d::BLEND_MODE::PRE_MUL);
	for(size_t object = 0; object < icon_count; object++) {
		for(size_t mode = 0; mode < 2; mode++) {
			// render object
			r->start_draw(buffer);
			r->clear();
			r->start_2d_draw();
			ui_theme->draw("editor_map_object",
						   (mode == 0 ? "normal" : "active"),
						   float2(0.0f), float2(object_texture_size), false, true,
						   [](const string& str a2e_unused){return "";},
						   [&object, &object_textures_3d, this](const string& tex_name a2e_unused) {
							   return object_textures_3d[object]->tex();
						   });
			r->stop_2d_draw();
			r->stop_draw();
			
			// copy texture to array
			glBindFramebuffer(GL_FRAMEBUFFER, buffer->resolve_buffer[0]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, buffer->tex[0], 0);
			
			glBindTexture(GL_TEXTURE_2D_ARRAY, object_texture_array);
			glCopyTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
								0, 0, (GLint)((object*2) + mode),
								0, 0, object_texture_size.x, object_texture_size.y);
			glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
		}
	}
	glDepthFunc(GL_LESS);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_BLEND);
	
	glBindTexture(GL_TEXTURE_2D_ARRAY, object_texture_array);
	glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	r->delete_buffer(buffer);
	
	for(auto& tex : object_textures_3d) {
		t->delete_texture(tex);
	}
	
	//
	glGenBuffers(1, &object_sprite_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, object_sprite_vbo);
	const auto sprite_points = array<float2, 4> {
		{
			float2(-0.5f, -0.5f),
			float2(-0.5f, 0.5f),
			float2(0.5f, -0.5f),
			float2(0.5f, 0.5f),
		},
	};
	glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(float2),
				 &sprite_points[0],
				 GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	glGenBuffers(1, &object_sprites_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, object_sprites_ubo);
	glBufferData(GL_UNIFORM_BUFFER, 65536, nullptr, GL_STATIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	
	//
	editor_light = new light(0.0f, 0.0f, 0.0f);
	editor_light->set_type(light::LIGHT_TYPE::DIRECTIONAL);
	editor_light->set_color(1.0f, 1.0f, 1.0f);
	editor_light->set_ambient(1.0f, 1.0f, 1.0f);
	
	//
	selection_menu_items = {
		{
			// blocks
			{ (size_t)BLOCK_MATERIAL::NONE, EDITOR_MODE::BLOCK_MODE },
			{ (size_t)BLOCK_MATERIAL::INDESTRUCTIBLE, EDITOR_MODE::BLOCK_MODE },
			{ (size_t)BLOCK_MATERIAL::METAL, EDITOR_MODE::BLOCK_MODE },
			{ (size_t)BLOCK_MATERIAL::MAGNET, EDITOR_MODE::BLOCK_MODE },
			{ (size_t)BLOCK_MATERIAL::LIGHT, EDITOR_MODE::BLOCK_MODE },
			{ (size_t)BLOCK_MATERIAL::ACID, EDITOR_MODE::BLOCK_MODE },
			{ (size_t)BLOCK_MATERIAL::SPRING, EDITOR_MODE::BLOCK_MODE },
			{ (size_t)BLOCK_MATERIAL::SPAWNER, EDITOR_MODE::BLOCK_MODE },
			
			// objects
			{ (size_t)OBJECT_TYPE::SOUND, EDITOR_MODE::OBJECT_MODE },
			{ (size_t)OBJECT_TYPE::MAP_LINK, EDITOR_MODE::OBJECT_MODE },
			{ (size_t)OBJECT_TYPE::TRIGGER, EDITOR_MODE::OBJECT_MODE },
			{ (size_t)OBJECT_TYPE::LIGHT_COLOR_AREA, EDITOR_MODE::OBJECT_MODE },
			{ (size_t)OBJECT_TYPE::AI_WAYPOINT, EDITOR_MODE::OBJECT_MODE },
		}
	};
	
	//
	eevt->add_internal_event_handler(event_handler_fnctr,
									 EVENT_TYPE::KEY_DOWN,
									 EVENT_TYPE::MOUSE_WHEEL_UP,
									 EVENT_TYPE::MOUSE_WHEEL_DOWN,
									 EVENT_TYPE::MOUSE_LEFT_DOWN,
									 EVENT_TYPE::MOUSE_LEFT_UP,
									 EVENT_TYPE::MOUSE_RIGHT_DOWN,
									 EVENT_TYPE::MOUSE_RIGHT_UP,
									 EVENT_TYPE::MOUSE_MOVE,
									 EVENT_TYPE::MOUSE_MIDDLE_UP);
}

editor::~editor() {
	eevt->remove_event_handler(event_handler_fnctr);
	
	sce->delete_light(editor_light);
	delete editor_light;
	
	for(auto& tex : object_textures) {
		t->delete_texture(tex);
		tex = nullptr;
	}
	
	if(enabled) set_enabled(false);
	if(glIsBuffer(cube_vbo)) glDeleteBuffers(1, &cube_vbo);
	if(glIsBuffer(cube_indices_vbo)) glDeleteBuffers(1, &cube_indices_vbo);
	
	if(glIsTexture(object_texture_array)) {
		glDeleteTextures(1, &object_texture_array);
	}
	if(glIsBuffer(object_sprites_ubo)) glDeleteBuffers(1, &object_sprites_ubo);
	if(glIsBuffer(object_sprite_vbo)) glDeleteBuffers(1, &object_sprite_vbo);
}

void editor::run() {
	static constexpr float y_offset = 32.0f;
	const float3 cam_pos(-cam->get_position());
	editor_light->set_position(cam_pos.x, cam_pos.y + y_offset, cam_pos.z);
}

void editor::draw_ui(const DRAW_MODE_UI draw_mode a2e_unused, rtt::fbo* buffer) {
	const float corner_radius = gui_theme::point_to_pixel(4.0f);
	const uint2 size(buffer->width, buffer->height);
	const rect editor_menu_rect(size.x - (unsigned int)(float(size.x)*0.06f),
								20,
								size.x + (unsigned int)ceilf(corner_radius),
								size.y - 20);
	const unsigned int max_width = size.x - editor_menu_rect.x1;
	
	// bg + outer line
	gfx2d::draw_rounded_rectangle_fill(editor_menu_rect, corner_radius, gfx2d::CORNER::ALL, float4(0.0f, 0.0, 0.0, 0.5f));
	gfx2d::draw_rounded_rectangle_border_fill(rect(editor_menu_rect.x1-1,
												   editor_menu_rect.y1-1,
												   editor_menu_rect.x2+1,
												   editor_menu_rect.y2+1),
											  corner_radius, gfx2d::CORNER::ALL, 4.5f, float4(0.1f, 0.1, 0.1, 0.8f));
	gfx2d::draw_rounded_rectangle_border_fill(editor_menu_rect, corner_radius, gfx2d::CORNER::ALL, 2.5f, float4(1.0f));
	
	//
	const uint2 offset(float(max_width) * 0.175f);
	const float tex_width = float(max_width) * 0.66f; // 0.15 from the left, 0.05 from the right, +0.05 on either side
	const unsigned int item_count = (unsigned int)selection_menu_items.size();
	const float tex_height_offset = float((editor_menu_rect.y2 - editor_menu_rect.y1) - offset.y*2
										  - item_count*tex_width) / float(item_count);
	
	//
	size_t item_counter = 0;
	for(const auto& item : selection_menu_items) {
		const float2 tex_offset(offset +
								float2(0.0f, float(item_counter++) * (tex_width + tex_height_offset)) +
								float2(editor_menu_rect.x1, editor_menu_rect.y1));
		const rect tex_rect(tex_offset.x, tex_offset.y, tex_offset.x + tex_width, tex_offset.y + tex_width);
		
		// block textures
		if(item.second == EDITOR_MODE::BLOCK_MODE) {
			gfx2d::draw_rounded_rectangle_texture(tex_rect, 4.0f, gfx2d::CORNER::ALL,
												  bt->mat_tex(sb_map::remap_material((BLOCK_MATERIAL)item.first), 0));
		}
		// object textures
		else {
			gfx2d::draw_rounded_rectangle_gradient(tex_rect, 4.0f, gfx2d::CORNER::ALL,
												   gfx2d::GRADIENT_TYPE::VERTICAL,
												   float4(0.0f, 1.0f, 0.0f, 0.0f),
												   vector<float4> {
													   float4(1.0f, 1.0f, 1.0f, 0.8f),
													   float4(0.6f, 0.6f, 0.6f, 0.8f)
												   });
			gfx2d::draw_rectangle_texture(tex_rect, object_textures[item.first]->tex());
		}
	}
	
	// draw selected item
	{
		const float2 tex_offset(offset + float2(0.0f, float(selected_item_index) * (tex_width + tex_height_offset))
								+ float2(editor_menu_rect.x1, editor_menu_rect.y1));
		const rect tex_rect(tex_offset.x, tex_offset.y, tex_offset.x + tex_width, tex_offset.y + tex_width);
		constexpr unsigned int selection_offset = 3;
		gfx2d::draw_rounded_rectangle_border_fill(rect(tex_rect.x1 - selection_offset,
													   tex_rect.y1 - selection_offset,
													   tex_rect.x2 + selection_offset,
													   tex_rect.y2 + selection_offset),
												  4.0f, gfx2d::CORNER::ALL, 2.0f, float4(1.0f, 0.0f, 0.0f, 1.0f));
	}
	
	// draw crosshair
	const float2 half_size(size / 2);
	const float crosshair_radius = 0.0045f * float(size.x);
	gfx2d::draw_circle_border_fill(half_size, crosshair_radius, crosshair_radius, 1.0f, float4(1.0f));
	gfx2d::draw_line_fill(float2(half_size.x - 0.5f, half_size.y - crosshair_radius), float2(half_size.x - 0.5f, half_size.y + crosshair_radius), float4(1.0f));
	gfx2d::draw_line_fill(float2(half_size.x - crosshair_radius, half_size.y - 0.5f), float2(half_size.x + crosshair_radius, half_size.y - 0.5f), float4(1.0f));
}

bool editor::event_handler(EVENT_TYPE type, shared_ptr<event_object> obj) {
	if(!enabled) return false;
	
	// handle scrolling -> switch to appropriate edit mode
	if(type == EVENT_TYPE::MOUSE_WHEEL_UP) {
		if(selected_item_index == 0) {
			selected_item_index = selection_menu_items.size() - 1;
		}
		else selected_item_index -= 1;
		selected_item = selection_menu_items[selected_item_index].first;
		ed->set_mode(selection_menu_items[selected_item_index].second);
	}
	else if(type == EVENT_TYPE::MOUSE_WHEEL_DOWN) {
		if(selected_item_index == (selection_menu_items.size() - 1)) {
			selected_item_index = 0;
		}
		else selected_item_index += 1;
		selected_item = selection_menu_items[selected_item_index].first;
		ed->set_mode(selection_menu_items[selected_item_index].second);
	}
	
	//
	if(editor_mode == EDITOR_MODE::BLOCK_MODE &&
	   cam->get_mouse_input()) {
		if(type == EVENT_TYPE::MOUSE_LEFT_DOWN) {
			start_selection(SELECTION_MODE::REMOVE);
		}
		else if(type == EVENT_TYPE::MOUSE_LEFT_UP) {
			end_selection(SELECTION_MODE::REMOVE);
		}
		else if(type == EVENT_TYPE::MOUSE_RIGHT_DOWN) {
			start_selection(SELECTION_MODE::CREATE);
		}
		else if(type == EVENT_TYPE::MOUSE_RIGHT_UP) {
			end_selection(SELECTION_MODE::CREATE);
		}
		else if(type == EVENT_TYPE::MOUSE_MOVE) {
			update_selection();
		}
	}
	//
	else if(editor_mode == EDITOR_MODE::OBJECT_MODE) {
		if(type == EVENT_TYPE::MOUSE_LEFT_UP) {
			// for now only react on left up events when the camera is enabled
			// also: disallow selection with an enabled workspace ui
			if((workspace_wnd == nullptr || workspace_wnd->get_children().empty()) &&
			   cam->get_mouse_input()) {
				const shared_ptr<mouse_left_up_event>& up_evt = (shared_ptr<mouse_left_up_event>&)obj;
				intersect_objects(cam->get_mouse_input() ? ipnt(-1) : up_evt->position);
				open_object_ui(object_selection.type, object_selection.get_ptr());
			}
		}
		else if(type == EVENT_TYPE::MOUSE_RIGHT_UP) {
			const auto intersection = intersect_static();
			if(!intersection.is_invalid()) {
				const uint3 block_pos = intersection.get_block_pos();
				if(active_map->is_valid_position(block_pos)) {
					bool object_at_position = false;
					// check for objects at the static intersection position (there shouldn't be two map objects at the same position)
					// cheap method of checking this, but it works ...
					for(size_t i = 0; i < active_sprites; i++) {
						const uint3 pos(object_sprites_ubo_data[i].floored());
						if((pos == block_pos).all()) {
							object_at_position = true;
							break;
						}
					}
					if(!object_at_position) {
						add_map_object(block_pos);
					}
				}
			}
		}
	}
	
	if(type == EVENT_TYPE::MOUSE_MIDDLE_UP ||
	   (type == EVENT_TYPE::KEY_DOWN && ((shared_ptr<key_down_event>&)obj)->key == SDLK_TAB)) {
		object_selection.reset();
		close_workspace_ui();
		switch_input_mode(cam->get_mouse_input() ^ true);
	}
	
	if(type == EVENT_TYPE::KEY_DOWN) {
		const shared_ptr<key_down_event>& key_evt = (shared_ptr<key_down_event>&)obj;
		if(editor_mode == EDITOR_MODE::OBJECT_MODE &&
		   ui->get_active_object() == nullptr && // makes sure no input box is active ...
		   (key_evt->key == SDLK_DELETE || key_evt->key == SDLK_BACKSPACE)) {
			// remove selected object
			switch(object_selection.type) {
				case OBJECT_TYPE::SOUND:
					active_map->remove_sound((audio_3d*)object_selection.sound);
					break;
				case OBJECT_TYPE::MAP_LINK:
					active_map->remove_map_link((sb_map::map_link*)object_selection.link);
					break;
				case OBJECT_TYPE::TRIGGER:
					active_map->remove_trigger((sb_map::trigger*)object_selection.trgr);
					break;
				case OBJECT_TYPE::LIGHT_COLOR_AREA:
					active_map->remove_light_color_area((sb_map::light_color_area*)object_selection.lca);
					break;
				case OBJECT_TYPE::AI_WAYPOINT:
					active_map->remove_ai_waypoint((sb_map::ai_waypoint*)object_selection.wp);
					break;
				default: break;
			}
			object_selection.reset();
			close_workspace_ui();
		}
	}
	
	// only needs to be redrawn when selection changes (mouse moves are irrelevant)
	if(type != EVENT_TYPE::MOUSE_MOVE) {
		ui_cb_obj->redraw();
	}
	
	return true;
}

void editor::start_selection(const SELECTION_MODE mode) {
	if(cur_selection_mode != SELECTION_MODE::NONE &&
	   mode != cur_selection_mode) return;
	// set current mode
	cur_selection_mode = mode;
	
	block_selection.first = selected_block; // currently selected block
	block_selection.second = selected_block;
}

void editor::end_selection(const SELECTION_MODE mode) {
	if(cur_selection_mode != SELECTION_MODE::NONE &&
	   mode != cur_selection_mode) return;
	
	if(selected_block.first.x != invalid_pos) {
		block_selection.second = selected_block;
	}
	
	// ... and modify:
	modify_selection(cur_selection_mode);
	
	// reset
	cur_selection_mode = SELECTION_MODE::NONE;
}

void editor::update_selection() {
	if(cur_selection_mode == SELECTION_MODE::NONE) return;
	
	if(selected_block.first.x != invalid_pos) {
		block_selection.second = selected_block;
	}
}

void editor::set_enabled(const bool state) {
	if(state == enabled) return;
	game_base::set_enabled(state);
	
	const bool inv_state = state ^ true;
	if(ge != nullptr) {
		ge->set_enabled(inv_state);
		ge->set_camera_control(inv_state);
		ge->set_event_input(inv_state);
	}
	cam->set_mouse_input(inv_state);
	cam->set_keyboard_input(inv_state);
	cam->set_wasd_input(false); // always disable this
	e->set_cursor_visible(state);
	
	if(state) {
		// disable console when in editor mode
		if(console->is_enabled()) {
			console->set_enabled(false);
		}
		
		ui_cb_obj = ui->add_draw_callback(DRAW_MODE_UI::PRE_UI, draw_cb, float2(1.0f), float2(0.0f));
		sce->add_draw_callback("editor_selection", scene_draw_cb);
		pc->halt_simulation();
		open_ui();
	}
	else {
		sce->delete_light(editor_light);
		ui_cb_obj = nullptr;
		ui->delete_draw_callback(draw_cb);
		sce->delete_draw_callback("editor_selection");
		pc->resume_simulation();
		close_ui();
	}
}

void editor::draw_selection(const DRAW_MODE draw_mode) {
	if(draw_mode != DRAW_MODE::MATERIAL_ALPHA_PASS) return;
	
	static constexpr float scale_size = 0.05f;
	if(editor_mode == EDITOR_MODE::BLOCK_MODE) {
		selected_block = intersect_static().block_ref;
		
		float4 selection_color(1.0f, 0.0f, 0.0f, 0.5f);
		matrix4f mvm;
		
		// single block:
		if(cur_selection_mode == SELECTION_MODE::NONE ||
		   block_selection.first.first.x == invalid_pos ||
		   block_selection.second.first.x == invalid_pos) {
			if(selected_block.first.x == invalid_pos) return;
			mvm = game_base::compute_block_view_matrix(selected_block.first, scale_size);
		}
		// multiple blocks:
		else {
			if(block_selection.first.first.x == invalid_pos ||
			   block_selection.second.first.x == invalid_pos) return;
			
			const float3 min(uint3::min(block_selection.first.first, block_selection.second.first));
			const float3 max(uint3::max(block_selection.first.first, block_selection.second.first));
			const float3 scale(float3(max) - float3(min) + float3(1.0f));
			
			// compute mvm
			mvm = matrix4f().scale(scale.x+scale_size, scale.y+scale_size, scale.z+scale_size);
			mvm *= *e->get_translation_matrix();
			mvm *= matrix4f().translate(min.x - scale_size*0.5f,
										min.y - scale_size*0.5f,
										min.z - scale_size*0.5f);
			mvm *= *e->get_rotation_matrix();
			
			selection_color = float4(1.0f, 0.3125f, 0.0f, 0.5f);
		}
		
		// draw
		gl3shader shd = s->get_gl3shader("SIMPLE");
		shd->use();
		shd->uniform("mvpm", mvm * *e->get_projection_matrix());
		shd->uniform("in_color", selection_color);
		
		// points
		shd->attribute_array("in_vertex", cube_vbo, 3);
		
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube_indices_vbo);
		glDrawElements(GL_TRIANGLES, (GLsizei)builtin_models::cube_index_count * 3, GL_UNSIGNED_BYTE, nullptr);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		
		// disable everything
		glDisableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		shd->disable();
	}

	// always draw map objects
	
	// update data
	{
		active_sprites = 0;
		const auto add_object = [this](const float3& position, const OBJECT_TYPE& type, void* ptr) {
			object_sprites_ubo_data[active_sprites++].set(position.x, position.y, position.z,
														  (float)(((unsigned int)type * 2) +
																  (object_selection.is_selected(ptr) ? 1 : 0)));
		};
		
		// env sounds
		for(const auto& snd : active_map->get_sounds()) {
			add_object(snd->get_position(), OBJECT_TYPE::SOUND, snd);
		}
		
		// map links
		for(const auto& ml : active_map->get_map_links()) {
			for(const auto& pos : ml->positions) {
				add_object(float3(pos) + 0.5f, OBJECT_TYPE::MAP_LINK, ml);
			}
		}
		
		// triggers
		for(const auto& trgr : active_map->get_triggers()) {
			add_object(float3(trgr->position) + 0.5f, OBJECT_TYPE::TRIGGER, trgr);
		}
		
		// light color areas
		for(const auto& lca : active_map->get_light_color_areas()) {
			add_object(float3(lca->min_pos) + 0.5f, OBJECT_TYPE::LIGHT_COLOR_AREA, lca);
		}
		
		// ai waypoints
		for(const auto& wp : active_map->get_ai_waypoints()) {
			add_object(float3(wp->position) + 0.5f, OBJECT_TYPE::AI_WAYPOINT, wp);
		}
		
		// player
		add_object(float3(active_map->get_player_start()) + 0.5f, OBJECT_TYPE::__MAX_OBJECT_TYPE, nullptr);
		
		glBindBuffer(GL_UNIFORM_BUFFER, object_sprites_ubo);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, active_sprites * sizeof(float4), &object_sprites_ubo_data[0]);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}
	
	// draw
	glDisable(GL_DEPTH_TEST);
	gl3shader shd = s->get_gl3shader("SB_EDITOR_SPRITES");
	shd->use();
	
	shd->uniform("mvpm", *e->get_mvp_matrix());
	shd->uniform("mvm", *e->get_modelview_matrix());
	shd->attribute_array("in_vertex", object_sprite_vbo, 2);
	shd->block("sprites", object_sprites_ubo);
	shd->texture("object_textures", object_texture_array, GL_TEXTURE_2D_ARRAY);
	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, active_sprites);
	
	shd->disable();
	glEnable(GL_DEPTH_TEST);
	
	// draw all light color areas if one is selected
	if(object_selection.type == OBJECT_TYPE::LIGHT_COLOR_AREA) {
		matrix4f mvm;
		gl3shader simple_shd = s->get_gl3shader("SIMPLE");
		simple_shd->use();
		
		// points
		simple_shd->attribute_array("in_vertex", cube_vbo, 3);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube_indices_vbo);
		
		for(const auto& lca : active_map->get_light_color_areas()) {
			float3 scale((lca->max_pos - lca->min_pos) + 1.0f);
			scale += scale_size * 2.0f;
			mvm = matrix4f().scale(scale.x, scale.y, scale.z);
			mvm *= *e->get_translation_matrix();
			mvm *= matrix4f().translate(lca->min_pos.x - scale_size, lca->min_pos.y - scale_size, lca->min_pos.z - scale_size);
			mvm *= *e->get_rotation_matrix();
			
			// draw
			simple_shd->uniform("mvpm", mvm * *e->get_projection_matrix());
			simple_shd->uniform("in_color", float4(lca->color, 0.5f));
			glDrawElements(GL_TRIANGLES, (GLsizei)builtin_models::cube_index_count * 3, GL_UNSIGNED_BYTE, nullptr);
		}
		
		// disable everything
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glDisableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		simple_shd->disable();
	}
}

void editor::modify_selection(const SELECTION_MODE mode) {
	if(active_map == nullptr) return;
	if(block_selection.first.first.x == invalid_pos ||
	   block_selection.second.first.x == invalid_pos) return;
	
	uint3 min(uint3::min(block_selection.first.first, block_selection.second.first));
	uint3 max(uint3::max(block_selection.first.first, block_selection.second.first));
	
	if(mode == SELECTION_MODE::REMOVE) {
		// don't allow the lowest layer to be removed, rather replace the cubes
		if(min.y == 0) {
			if(selected_item != (size_t)BLOCK_MATERIAL::NONE) {
				active_map->update(make_pair(min, uint3(max.x, 0, max.z)), (BLOCK_MATERIAL)selected_item);
			}
			
			// continue ...
			min.y++;
		}
		
		// continue by removing all other blocks (unless there aren't any left)
		if(max.y == 0) return;
		active_map->update(min, BLOCK_MATERIAL::NONE);
	}
	else if(mode == SELECTION_MODE::CREATE) {
		// single block create:
		if((min == max).all()) {
			// compute block position depending on block face
			int3 faced_cube_pos(selected_block.second == BLOCK_FACE::RIGHT ? 1 :
								(selected_block.second == BLOCK_FACE::LEFT ? -1 : 0),
								selected_block.second == BLOCK_FACE::TOP ? 1 :
								(selected_block.second == BLOCK_FACE::BOTTOM ? -1 : 0),
								selected_block.second == BLOCK_FACE::BACK ? 1 :
								(selected_block.second == BLOCK_FACE::FRONT ? -1 : 0));
			faced_cube_pos += int3(selected_block.first);
			
			// fail if any element is negative
			if(faced_cube_pos.min_element() < 0 ||
			   !active_map->is_valid_position(faced_cube_pos)) return;
			
			active_map->update(faced_cube_pos, (BLOCK_MATERIAL)selected_item);
		}
		// multiple block set/create:
		else {
			active_map->update(make_pair(min, max), (BLOCK_MATERIAL)selected_item);
		}
	}
}

void editor::set_mode(const EDITOR_MODE mode) {
	if(mode == editor_mode) return;
	editor_mode = mode;
	selected_item = selection_menu_items[selected_item_index].first;
	if(ui_cb_obj != nullptr) ui_cb_obj->redraw();
}

const editor::EDITOR_MODE& editor::get_mode() const {
	return editor_mode;
}

editor::OBJECT_TYPE editor::intersect_objects(ipnt wnd_pos a2e_unused) {
	object_selection.reset();
	
	// compute camera forward vector
	const matrix4d mview(*e->get_rotation_matrix());
	const matrix4d mproj(matrix4d().perspective((double)e->get_fov(), double(e->get_width()) / double(e->get_height()),
												(double)e->get_near_far_plane().x, (double)e->get_near_far_plane().y));
	const matrix4d ipm((mview * mproj).invert());
	const double3 proj_pos(double3(0.0, 0.0, 1.0) * ipm);
	
	// create ray from camera to this point
	const ray sel_line(-*e->get_position(), proj_pos.normalized());
	
	float min_intersection = std::numeric_limits<float>::max();
	const auto intersect_object = [&sel_line, &min_intersection](const float3& position) -> bool {
		static const float3 box_half_size(0.5f);
		const bbox object_bbox(position - box_half_size, position + box_half_size);
		pair<float, float> ret;
		object_bbox.intersect(ret, sel_line);
		if(ret.first > 0.0f && ret.first < ret.second && ret.first < min_intersection) {
			min_intersection = ret.first;
			return true;
		}
		return false;
	};
	
	// env sounds
	for(const auto& snd : active_map->get_sounds()) {
		if(intersect_object(snd->get_position())) {
			object_selection.reset();
			object_selection.type = OBJECT_TYPE::SOUND;
			object_selection.sound = snd;
		}
	}
	
	// map links
	for(const auto& ml : active_map->get_map_links()) {
		for(const auto& pos : ml->positions) {
			if(intersect_object(float3(pos) + 0.5f)) {
				object_selection.reset();
				object_selection.type = OBJECT_TYPE::MAP_LINK;
				object_selection.link = ml;
			}
		}
	}
	
	// trigger
	for(const auto& trgr : active_map->get_triggers()) {
		if(intersect_object(float3(trgr->position) + 0.5f)) {
			object_selection.reset();
			object_selection.type = OBJECT_TYPE::TRIGGER;
			object_selection.trgr = trgr;
		}
	}
	
	// light color areas
	for(const auto& lca : active_map->get_light_color_areas()) {
		if(intersect_object(float3(lca->min_pos) + 0.5f)) {
			object_selection.reset();
			object_selection.type = OBJECT_TYPE::LIGHT_COLOR_AREA;
			object_selection.lca = lca;
		}
	}
	
	// ai waypoints
	for(const auto& wp : active_map->get_ai_waypoints()) {
		if(intersect_object(float3(wp->position) + 0.5f)) {
			object_selection.reset();
			object_selection.type = OBJECT_TYPE::AI_WAYPOINT;
			object_selection.wp = wp;
		}
	}
	
	return object_selection.type;
}

void editor::add_map_object(const uint3& block_pos) {
	object_selection.reset();
	switch((OBJECT_TYPE)selected_item) {
		case OBJECT_TYPE::SOUND: {
			// find/generate an unused identifier
			string identifier = "";
			for(;;) {
				identifier = int2string(core::rand(numeric_limits<int>::max()));
				if(ac->get_audio_source(identifier) != nullptr) continue;
				else break;
			}
			audio_3d* snd = ac->add_audio_3d("NONE", identifier);
			if(snd == nullptr) {
				a2e_error("failed to add 3d audio \"NONE.%s\"!", identifier);
				break;
			}
			snd->set_position(float3(block_pos) + 0.5f);
			active_map->add_sound(snd);
			snd->stop();
		}
		break;
		case OBJECT_TYPE::MAP_LINK: {
			sb_map::map_link* ml = new sb_map::map_link {
				"intro.map",
				"NEXT",
				vector<uint3> {
					block_pos
				},
				true
			};
			active_map->add_map_link(ml);
			object_selection.type = OBJECT_TYPE::MAP_LINK;
			object_selection.link = ml;
		}
		break;
		case OBJECT_TYPE::TRIGGER: {
			sb_map::trigger* trgr = new sb_map::trigger {
				"TRIGGER",
				TRIGGER_TYPE::NONE,
				TRIGGER_SUB_TYPE::NONE,
				block_pos,
				BLOCK_FACE::ALL,
				sh->load_script("global.script"),
				"",
				"",
				"",
				// dependent data:
				0.0f,
				0.0f,
				0,
				sb_map::trigger::state_struct()
			};
			active_map->add_trigger(trgr);
			object_selection.type = OBJECT_TYPE::TRIGGER;
			object_selection.trgr = trgr;
		}
		break;
		case OBJECT_TYPE::LIGHT_COLOR_AREA: {
			sb_map::light_color_area* lca = new sb_map::light_color_area {
				block_pos,
				block_pos,
				float3(1.0f)
			};
			active_map->add_light_color_area(lca);
			object_selection.type = OBJECT_TYPE::LIGHT_COLOR_AREA;
			object_selection.lca = lca;
		}
		break;
		case OBJECT_TYPE::AI_WAYPOINT: {
			sb_map::ai_waypoint* wp = new sb_map::ai_waypoint {
				int2string(core::rand(1000)),
				nullptr,
				block_pos
			};
			active_map->add_ai_waypoint(wp);
			object_selection.type = OBJECT_TYPE::AI_WAYPOINT;
			object_selection.wp = wp;
		}
		break;
		default: break;
	}
	open_object_ui(object_selection.type, object_selection.get_ptr());
}
