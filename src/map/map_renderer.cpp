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

#include "map_renderer.h"
#include "block_textures.h"
#include "builtin_models.h"
#include "weight_slider.h"
#include "game_base.h"
#include <scene/camera.h>
#include <core/quaternion.h>
#include <particle/particle.h>
#include <rendering/gl_timer.h>

map_renderer::map_renderer() :
a2estatic(::e, ::s, ::sce),
map_event_handler_fctr(this, &map_renderer::map_event_handler)
{
	// create cube and vbos
	
	game_base::create_cube(draw_vertices_vbo, draw_indices_vbo);
	game_base::create_cube_tex_n_bn_tn(draw_tex_coords_vbo, draw_normals_vbo, draw_binormals_vbo, draw_tangents_vbo);

	// culling info vbo
	glGenBuffers(1, &draw_culling_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, draw_culling_vbo);
	glBufferData(GL_ARRAY_BUFFER, builtin_models::cube_vertex_count * sizeof(unsigned int), &builtin_models::cube_culling[0], GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	draw_index_count = builtin_models::cube_index_count * 3;	
	// reset buffer
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	sce->add_model(this);
	
	//
	push_button_mat = { { new a2ematerial(e), new a2ematerial(e) } };
	push_button_mat[0]->load_material(e->data_path("push_button_off.a2mtl"));
	push_button_mat[1]->load_material(e->data_path("push_button_on.a2mtl"));
	
	push_button = sce->create_a2emodel<a2estatic>();
	push_button->load_model(e->data_path("push_button.a2m"));
	push_button->set_material(push_button_mat[0]);
	
	weight_trigger_mat = { { new a2ematerial(e), new a2ematerial(e) } };
	weight_trigger_mat[0]->load_material(e->data_path("weight_trigger_off.a2mtl"));
	weight_trigger_mat[1]->load_material(e->data_path("weight_trigger_on.a2mtl"));
	
	weight_trigger = sce->create_a2emodel<a2estatic>();
	weight_trigger->load_model(e->data_path("weight_trigger.a2m"));
	weight_trigger->set_material(weight_trigger_mat[0]);
	
	light_trigger_mat = { { new a2ematerial(e), new a2ematerial(e) } };
	light_trigger_mat[0]->load_material(e->data_path("light_trigger_off.a2mtl"));
	light_trigger_mat[1]->load_material(e->data_path("light_trigger_on.a2mtl"));
	
	light_trigger = sce->create_a2emodel<a2estatic>();
	light_trigger->load_model(e->data_path("light_trigger.a2m"));
	light_trigger->set_material(light_trigger_mat[0]);
	
	//
	particle_textures = {
		{ "DOOR", t->add_texture(e->data_path("particle.png"), e->get_filtering(), e->get_anisotropic(),
								 GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE) },
		{ "ACID", t->add_texture(e->data_path("particle.png"), e->get_filtering(), e->get_anisotropic(),
								 GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE) },
		{ "AI_ATTACK", t->add_texture(e->data_path("particle.png"), e->get_filtering(), e->get_anisotropic(),
								 GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE) },
	};
	
	//
	eevt->add_internal_event_handler(map_event_handler_fctr, EVENT_TYPE::MAP_LOAD, EVENT_TYPE::MAP_UNLOAD);
}

map_renderer::~map_renderer() {
	eevt->remove_event_handler(map_event_handler_fctr);
	sce->delete_model(this);
	
	//
	delete push_button;
	delete push_button_mat[0];
	delete push_button_mat[1];
	
	delete weight_trigger;
	delete weight_trigger_mat[0];
	delete weight_trigger_mat[1];
	
	delete light_trigger;
	delete light_trigger_mat[0];
	delete light_trigger_mat[1];
	
	for(auto& tex : particle_textures) {
		t->delete_texture(tex.second);
	}
	
	if(glIsBuffer(draw_culling_vbo)) {
		glDeleteBuffers(1, &draw_culling_vbo);
	}
}

a2e_texture map_renderer::get_particle_texture(const string& name) {
	const auto iter = particle_textures.find(name);
	if(iter == particle_textures.end()) return nullptr;
	return iter->second;
}

bool map_renderer::map_event_handler(EVENT_TYPE type, shared_ptr<event_object> obj a2e_unused) {
	if(type == EVENT_TYPE::MAP_LOAD) {
		if(pm == nullptr) return true;
		
		e->acquire_gl_context();
		
		// check where particle systems need to be set
		const auto& chunks(active_map->get_chunks());
		const auto& chunk_count(active_map->get_chunk_count());
		vector<pair<uint3, uint3>> ignore_blocks;
		for(unsigned int cx = 0; cx < chunk_count.x; cx++) {
			for(unsigned int cz = 0; cz < chunk_count.z; cz++) {
				for(unsigned int cy = 0; cy < chunk_count.y; cy++) {
					const uint3 chunk_pos(uint3(cx, cy, cz) * sb_map::chunk_extent);
					const auto& chunk(chunks[active_map->chunk_position_to_index(uint3(cx, cy, cz))]);
					for(unsigned int block_idx = 0; block_idx < sb_map::blocks_per_chunk; block_idx++) {
						switch(chunk[block_idx].material) {
							case BLOCK_MATERIAL::ACID: {
								const uint3 cur_pos(chunk_pos + sb_map::block_index_to_position(block_idx));
								bool ignore_block = false;
								for(const auto& ign_block : ignore_blocks) {
									if(cur_pos.y >= ign_block.first.y && cur_pos.y <= ign_block.second.y &&
									   cur_pos.x >= ign_block.first.x && cur_pos.x <= ign_block.second.x &&
									   cur_pos.z >= ign_block.first.z && cur_pos.z <= ign_block.second.z) {
										ignore_block = true;
										break;
									}
								}
								if(ignore_block) break;
								uint3 bmin(cur_pos), bmax(cur_pos);
								
								// note: this will only look for other acid blocks on the min/max border
								bool found = true;
								while(found) {
									found = false;
									if(active_map->is_valid_position(int3(bmin) + int3(-1, 0, 0))) {
										if(active_map->get_block(uint3(bmin.x - 1, bmin.y, bmin.z)).material == BLOCK_MATERIAL::ACID) {
											bmin.x--;
											found = true;
										}
									}
									if(active_map->is_valid_position(int3(bmin) + int3(0, 0, -1))) {
										if(active_map->get_block(uint3(bmin.x, bmin.y, bmin.z - 1)).material == BLOCK_MATERIAL::ACID) {
											bmin.z--;
											found = true;
										}
									}
									if(active_map->is_valid_position(int3(bmax) + int3(1, 0, 0))) {
										if(active_map->get_block(uint3(bmax.x + 1, bmax.y, bmax.z)).material == BLOCK_MATERIAL::ACID) {
											bmax.x++;
											found = true;
										}
									}
									if(active_map->is_valid_position(int3(bmax) + int3(1, 0, 1))) {
										if(active_map->get_block(uint3(bmax.x, bmax.y, bmax.z + 1)).material == BLOCK_MATERIAL::ACID) {
											bmax.z++;
											found = true;
										}
									}
								}
								
								// check all x/z blocks on the y layers below and above (if there are all acid blocks, extend)
								if(bmin.x != bmax.x || bmin.z != bmax.z) { // this doesn't work on single acid blocks
									for(int by = bmin.y - 1; by >= 0; by--) {
										bool inc_layer = true;
										for(unsigned int bx = bmin.x; bx < bmax.x; bx++) {
											for(unsigned int bz = bmin.z; bz < bmax.z; bz++) {
												if(active_map->get_block(uint3(bx, by, bz)).material != BLOCK_MATERIAL::ACID) {
													bx = bmax.x;
													by = 0;
													inc_layer = false;
													break;
												}
											}
										}
										if(inc_layer) {
											bmin.y = by;
										}
										else break;
									}
									for(unsigned int by = bmax.y+1; by < (chunk_count.y * sb_map::chunk_extent); by++) {
										bool inc_layer = true;
										for(unsigned int bx = bmin.x; bx < bmax.x; bx++) {
											for(unsigned int bz = bmin.z; bz < bmax.z; bz++) {
												if(active_map->get_block(uint3(bx, by, bz)).material != BLOCK_MATERIAL::ACID) {
													bx = bmax.x;
													by = 0;
													inc_layer = false;
													break;
												}
											}
										}
										if(inc_layer) {
											bmax.y = by;
										}
										else break;
									}
								}
								ignore_blocks.push_back(make_pair(bmin, bmax));
								
								// acid system:
								const size_t block_count((bmax.x - bmin.x + 1) * (bmax.z - bmin.z + 1) * (bmax.y - bmin.y + 1));
								const size_t particles_per_block = 8;
								float3 extents(bmax - bmin + 1);
								extents.y *= 0.5f;
								float3 pos(float3(bmin) + float3(extents.x * 0.5f, extents.y * 1.25f, extents.z * 0.5f));
								particle_system* ps = pm->add_particle_system(particle_system::EMITTER_TYPE::BOX,
																			  particle_system::LIGHTING_TYPE::NONE,
																			  particle_textures["ACID"],
																			  block_count * particles_per_block,
																			  2000,
																			  1.0f,
																			  pos,
																			  float3(0.0f),
																			  extents,
																			  float3(0.0f, 1.0, 0.0f),
																			  float3(DEG2RAD(20.0f), DEG2RAD(20.0f), 0.0f),
																			  float3(0.0f, -1.0f, 0.0f),
																			  float4(0.25f, 0.7f, 0.1f, 0.125f),
																			  float2(1.5f));
								particles.push_back(ps);
							}
							break;
							default: break;
						}
					}
				}
			}
		}
		
		for(const auto& ml : active_map->get_map_links()) {
			uint3 bmin(numeric_limits<unsigned int>::max()), bmax(numeric_limits<unsigned int>::min());
			for(const auto& pos : ml->positions) {
				bmin.min(pos);
				bmax.max(pos);
			}
			const uint3 extents(bmax - bmin + 1);
			const float3 pos(float3(bmin) + float3(extents) * 0.5f);
			
			particle_system* ps = pm->add_particle_system(particle_system::EMITTER_TYPE::BOX,
														  particle_system::LIGHTING_TYPE::NONE,
														  particle_textures["DOOR"],
														  4096,
														  1000,
														  1.0f,
														  pos,
														  float3(0.0f),
														  extents,
														  float3(0.0f, 1.0, 0.0f),
														  float3(0.0f),
														  float3(0.0f, 0.1f, 0.0f),
														  float4(0.0f, 0.7f, 1.0f, 0.125f),
														  float2(0.075f, 1.0f));
			particles.push_back(ps);
			ml_particles.insert(make_pair(ml, ps));
			ps->set_active(ml->enabled);
			ps->set_visible(ml->enabled);
		}
		
		e->release_gl_context();
		return true;
	}
	else if(type == EVENT_TYPE::MAP_UNLOAD) {
		if(pm == nullptr) return true;
		
		e->acquire_gl_context();
		
		// clear particle systems
		for(const auto& ps : particles) {
			pm->delete_particle_system(ps);
		}
		particles.clear();
		ml_particles.clear();
		e->release_gl_context();
		return true;
	}
	return false;
}

void map_renderer::draw(const DRAW_MODE draw_mode) {
	if(active_map == nullptr) return;
	
	const DRAW_MODE masked_draw_mode((DRAW_MODE)((unsigned int)draw_mode & (unsigned int)DRAW_MODE::GM_PASSES_MASK));
	const bool env_pass(((unsigned int)draw_mode & (unsigned int)DRAW_MODE::ENVIRONMENT_PASS) != 0);
	if(draw_mode == DRAW_MODE::NONE ||
	   draw_mode > DRAW_MODE::ENV_GM_PASSES_MASK) {
		a2e_error("invalid draw_mode: %u!", draw_mode);
		return;
	}
	if(masked_draw_mode == DRAW_MODE::GEOMETRY_ALPHA_PASS ||
	   masked_draw_mode == DRAW_MODE::MATERIAL_ALPHA_PASS) {
		return; // no alpha for now
	}
	
	a2emodel::pre_draw_setup();
	
	if(masked_draw_mode == DRAW_MODE::MATERIAL_PASS) gl_timer::mark("MAP_START");
	// type:0 = static map, type:1 = dynamic map
	for(size_t map_type = 0; map_type < 2; map_type++) {
		// inferred rendering
		gl3shader shd;
		const string shd_option = (masked_draw_mode == DRAW_MODE::GEOMETRY_PASS ||
								   masked_draw_mode == DRAW_MODE::MATERIAL_PASS ?
								   "opaque" : "alpha");
		set<string> shd_combiners;
		if(env_pass) shd_combiners.insert("*env_probe");
		if(map_type == 1) shd_combiners.insert("*dynamic");
		if(!culling) shd_combiners.insert("*no_cull");
		
		if(masked_draw_mode == DRAW_MODE::GEOMETRY_PASS ||
		   masked_draw_mode == DRAW_MODE::GEOMETRY_ALPHA_PASS) {
			// first, select shader dependent on material type
			shd = s->get_gl3shader("SB_GP_INSTANCED_MAP");
			shd->use(shd_option, shd_combiners);
		}
		else if(masked_draw_mode == DRAW_MODE::MATERIAL_PASS ||
				masked_draw_mode == DRAW_MODE::MATERIAL_ALPHA_PASS) {
			// first, select shader dependent on material type
			shd = s->get_gl3shader("SB_MP_INSTANCED_MAP");
			shd->use(shd_option, shd_combiners);
			
			// inferred rendering setup
			ir_mp_setup(shd, shd_option, shd_combiners);
		}
		
		//
		if(masked_draw_mode == DRAW_MODE::GEOMETRY_PASS) {
			shd->texture("normal_textures", bt->tex_id(3), GL_TEXTURE_2D_ARRAY);
			shd->texture("height_textures", bt->tex_id(4), GL_TEXTURE_2D_ARRAY);
			shd->texture("aux_textures", bt->tex_id(5), GL_TEXTURE_2D_ARRAY);
		}
		else if(masked_draw_mode == DRAW_MODE::MATERIAL_PASS) {
			shd->texture("diffuse_textures", bt->tex_id(0), GL_TEXTURE_2D_ARRAY);
			shd->texture("specular_textures", bt->tex_id(1), GL_TEXTURE_2D_ARRAY);
			shd->texture("reflectance_textures", bt->tex_id(2), GL_TEXTURE_2D_ARRAY);
			shd->texture("height_textures", bt->tex_id(4), GL_TEXTURE_2D_ARRAY);
		}
		
		shd->uniform("cam_position", -float3(*e->get_position()));
		
		if(env_pass) {
			quaternionf q_x, q_y;
			q_x.set_rotation(e->get_rotation()->x, float3(1.0f, 0.0f, 0.0f));
			q_y.set_rotation(180.0f - e->get_rotation()->y, float3(0.0f, 1.0f, 0.0f));
			q_y *= q_x;
			q_y.normalize();
			matrix4f mvm_backside(q_y.to_matrix4());
			mvm_backside = *e->get_translation_matrix() * mvm_backside;
			shd->uniform("mvpm_backside", mvm_backside * *e->get_projection_matrix());
		}
		shd->uniform("mvpm", mvpm);
		
		shd->attribute_array("in_vertex", draw_vertices_vbo, 3);
		shd->attribute_array("texture_coord", draw_tex_coords_vbo, 2);
		shd->attribute_array("normal", draw_normals_vbo, 3);
		shd->attribute_array("binormal", draw_binormals_vbo, 3);
		shd->attribute_array("tangent", draw_tangents_vbo, 3);
		
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, draw_indices_vbo);
		
		if(map_type == 0) {
			if(culling) shd->attribute_array("in_culling", draw_culling_vbo, 1, GL_UNSIGNED_INT);
			
			unsigned int chunk_counter = 0;
			for(const auto& chunk : active_map->get_render_chunks()) {
				shd->uniform("offset", chunk.offset);
				shd->block("blocks", chunk.ubo);
				glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)draw_index_count, GL_UNSIGNED_BYTE, nullptr, sb_map::blocks_per_chunk);
				chunk_counter++;
			}
		}
		else {
			const pair<GLuint, size_t> render_data(active_map->get_dynamic_render_data());
			shd->block("blocks", render_data.first);
			glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)draw_index_count, GL_UNSIGNED_BYTE, nullptr, (GLsizei)render_data.second);
		}
		
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		
		shd->disable();
		if(masked_draw_mode == DRAW_MODE::MATERIAL_PASS) gl_timer::mark("TYPE #"+size_t2string(map_type)+" END");
	}
	if(masked_draw_mode == DRAW_MODE::MATERIAL_PASS) gl_timer::mark("MAP_END");
	
	// draw triggers
	push_button->set_ir_buffers(g_buffer, l_buffer,
								g_buffer_alpha, l_buffer_alpha);
	weight_trigger->set_ir_buffers(g_buffer, l_buffer,
								   g_buffer_alpha, l_buffer_alpha);
	light_trigger->set_ir_buffers(g_buffer, l_buffer,
								  g_buffer_alpha, l_buffer_alpha);
	for(const auto& trgr : active_map->get_triggers()) {
		static const unordered_map<unsigned int, float3> facing_offsets {
			{ (unsigned int)BLOCK_FACE::RIGHT, float3(1.0f, 0.5f, 0.5f) },
			{ (unsigned int)BLOCK_FACE::LEFT, float3(0.0f, 0.5f, 0.5f) },
			{ (unsigned int)BLOCK_FACE::TOP, float3(0.5f, 1.0f, 0.5f) },
			{ (unsigned int)BLOCK_FACE::BOTTOM, float3(0.5f, 0.0f, 0.5f) },
			{ (unsigned int)BLOCK_FACE::BACK, float3(0.5f, 0.5f, 1.0f) },
			{ (unsigned int)BLOCK_FACE::FRONT, float3(0.5f, 0.5f, 0.0f) },
		};
		static const unordered_map<unsigned int, float3> facing_rotations {
			{ (unsigned int)BLOCK_FACE::RIGHT, float3(0.0f, 0.0f, 90.0f) },
			{ (unsigned int)BLOCK_FACE::LEFT, float3(0.0f, 0.0f, -90.0f) },
			{ (unsigned int)BLOCK_FACE::TOP, float3(0.0f, 0.0f, 0.0f) },
			{ (unsigned int)BLOCK_FACE::BOTTOM, float3(180.0f, 0.0f, 0.0f) },
			{ (unsigned int)BLOCK_FACE::BACK, float3(-90.0f, 0.0f, 0.0f) },
			{ (unsigned int)BLOCK_FACE::FRONT, float3(90.0f, 0.0f, 0.0f) },
		};
		switch(trgr->type) {
			case TRIGGER_TYPE::PUSH:
				for(unsigned int facing = (unsigned int)BLOCK_FACE::RIGHT; facing <= (unsigned int)BLOCK_FACE::FRONT; facing <<= 1) {
					if(((unsigned int)trgr->facing & facing) == 0) continue;
					push_button->set_position(float3(trgr->position) + facing_offsets.at(facing));
					push_button->set_rotation(facing_rotations.at(facing));
					push_button->set_material(push_button_mat[trgr->state.active ? 1 : 0]);
					push_button->draw(draw_mode);
				}
				break;
			case TRIGGER_TYPE::WEIGHT:
				weight_trigger->set_position(float3(trgr->position) + float3(0.5f, 0.0f, 0.5f));
				weight_trigger->set_material(weight_trigger_mat[trgr->state.active ? 1 : 0]);
				weight_trigger->draw(draw_mode);
				break;
			case TRIGGER_TYPE::LIGHT:
				for(unsigned int facing = (unsigned int)BLOCK_FACE::RIGHT; facing <= (unsigned int)BLOCK_FACE::FRONT; facing <<= 1) {
					if(((unsigned int)trgr->facing & facing) == 0) continue;
					light_trigger->set_position(float3(trgr->position) + facing_offsets.at(facing));
					light_trigger->set_rotation(facing_rotations.at(facing));
					light_trigger->set_material(light_trigger_mat[trgr->state.active ? 1 : 0]);
					light_trigger->draw(draw_mode);
				}
				break;
			default:
				break;
		}
	}
	
	// set/check map_link and particle system activity
	for(const auto& ml : active_map->get_map_links()) {
		if(ml_particles.count(ml) > 0) {
			ml_particles[ml]->set_active(ml->enabled);
			ml_particles[ml]->set_visible(ml->enabled);
		}
	}
	
	a2emodel::post_draw_setup();
}

void map_renderer::set_culling(const bool& state) {
	culling = state;
}
