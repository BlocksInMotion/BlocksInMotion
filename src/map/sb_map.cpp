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

#include "sb_map.h"
#include "physics_controller.h"
#include "rigid_body.h"
#include "soft_body.h"
#include "game.h"
#include "audio_controller.h"
#include "ai_entity.h"
#include "script_handler.h"
#include "script.h"
#include "weight_slider.h"
#include "map_storage.h"
#include "save.h"
#include <rendering/extensions.h>
#include <scene/scene.h>
#include <scene/camera.h>

constexpr unsigned int sb_map::map_version;
constexpr size_t sb_map::chunk_extent;
constexpr size_t sb_map::blocks_per_chunk;

sb_map::sb_map(const string& filename_) :
filename(filename_),
block_rinfo(&pc->add_rigid_info<physics_controller::SHAPE::BOX>(0.0f, float3(0.5f))),
evt_handler_fnctr(this, &sb_map::event_handler)
{
	dynamic_render_data_size = 0;
	glGenBuffers(1, &dynamic_bodies_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, dynamic_bodies_ubo);
	glBufferData(GL_UNIFORM_BUFFER, 65536, nullptr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	
	eevt->add_event_handler(evt_handler_fnctr,
							EVENT_TYPE::PLAYER_STEP, EVENT_TYPE::PLAYER_BLOCK_STEP,
							EVENT_TYPE::AI_STEP, EVENT_TYPE::AI_BLOCK_STEP);
}

sb_map::~sb_map() {
	eevt->remove_event_handler(evt_handler_fnctr);
	
	render_chunks.clear();
	if(glIsBuffer(dynamic_bodies_ubo)) glDeleteBuffers(1, &dynamic_bodies_ubo);
	
	for(const auto& light_container : lights) {
		for(const auto& l : light_container) {
			sce->delete_light(l.second);
			delete l.second;
		}
	}
	lights.clear();
	
	pc->lock();
	for(const auto& sp : springs) {
		// must be called before killing all other dynamic_bodies!
		dynamic_bodies.erase(sp->body);
		pc->remove_rigid_body(sp->body);
		pc->remove_rigid_info(sp->info);
		delete sp;
	}
	pc->unlock();
	
	for(const auto& sbody_container : static_bodies) {
		for(const auto& body : sbody_container) {
			pc->remove_rigid_body(body.second);
		}
	}
	static_bodies.clear();
	dynamic_body_field.clear(); // note: already deleted by static_bodies
	
	for(const auto& dbody : dynamic_bodies) {
		pc->remove_rigid_body(dbody.first);
	}
	
	if(background_music != nullptr) {
		ac->delete_audio_source(background_music->get_identifier());
	}
	for(const auto& sound : env_sounds) {
		ac->delete_audio_source(sound->get_identifier());
	}
	
	for(const auto& lca : light_color_areas) {
		delete lca;
	}
	for(const auto& ml : map_links) {
		delete ml;
	}
	
	for(const auto& entity : entities) {
		delete entity;
	}
	
	for(const auto& spwn : spawners) {
		delete spwn;
	}
	
	for(const auto& trgr : triggers) {
		if(trgr->type == TRIGGER_TYPE::WEIGHT) {
			pc->remove_weight_slider(trgr->state.slider);
		}
		delete trgr;
	}
	
	for(const auto& wp : ai_waypoints) {
		delete wp;
	}
	ai_waypoints.clear();
}

void sb_map::run() {
	ge->graphics_update();
	for(const auto& entity : entities) {
		entity->graphics_update();
	}
	
	// spring handling
	vector<spring*> del_springs;
	pc->lock();
	const unsigned int cur_ticks(SDL_GetTicks());
	for(const auto& sp : springs) {
		static const unsigned int ext_time = 3000;
		const float step_size = 0.0025f; // extension / ms
		sp->scale = core::clamp((sp->state >= 0.0f ? 0.0f : 1.0f) + sp->state * step_size * float(cur_ticks - sp->timer), 0.0f, 1.0f);
		
		const float3 dir_abs(sp->direction.abs());
		// non-dir scale: 1.0, dir scale: [0, 2]
		sp->body->set_scale((float3(1.0f) - dir_abs) + dir_abs * sp->scale * 2.0f);
		// start off by one block into the extension direction (on the spring block side), then accommodate for scale
		sp->body->set_position(float3(sp->position) + (float3(1.0f) + sp->direction) * 0.5f + sp->direction * sp->scale);
		
		if(sp->scale == 1.0f && (cur_ticks - sp->timer) > ext_time) {
			// fully extended and extension period is over -> retract
			sp->state = -sp->state;
			sp->timer = cur_ticks;
			play_sound("SPRING", sp->position);
		}
		else if(sp->scale == 0.0f) {
			// fully retracted and retraction period is over -> delete
			del_springs.push_back(sp);
		}
	}
	for(const auto& sp : del_springs) {
		const auto iter = find(begin(springs), end(springs), sp);
		if(iter != end(springs)) {
			springs.erase(iter);
		}
		dynamic_bodies.erase(sp->body);
		pc->remove_rigid_body(sp->body);
		pc->remove_rigid_info(sp->info);
		delete sp;
	}
	pc->unlock();
	
	// trigger handling
	for(auto& trgr : triggers) {
		if(trgr->sub_type == TRIGGER_SUB_TYPE::TIMED &&
		   trgr->state.timer != 0 && trgr->state.timer < cur_ticks) {
			trgr->deactivate(this);
		}
		
		if(trgr->type == TRIGGER_TYPE::WEIGHT) {
			const bool triggered(trgr->state.slider->is_triggered());
			if(!trgr->state.active && triggered) {
				trgr->activate(this);
			}
			else if(trgr->state.active && !triggered) {
				trgr->deactivate(this);
			}
		}
	}
	
	// spawner handling
	for(const auto& spwn : spawners) {
		// check if an ai entity must be spawned
		if(spwn->spawns.size() < spwn->max_spawns) {
			// find a valid direction/position
			static const array<int3, 6> offsets {
				{
					int3(1, 0, 0),
					int3(-1, 0, 0),
					int3(0, 1, 0),
					int3(0, -1, 0),
					int3(0, 0, 1),
					int3(0, 0, -1),
				}
			};
			vector<uint3> valid_positions;
			for(size_t i = 0; i < offsets.size(); i++) {
				const uint3 pos(spwn->position + offsets[i]);
				if(is_valid_position(pos)) {
					if(get_block(pos).material == BLOCK_MATERIAL::NONE) {
						valid_positions.push_back(pos);
					}
				}
			}
			if(!valid_positions.empty()) {
				const uint3 rand_pos(valid_positions[core::rand((int)valid_positions.size())]);
				spwn->spawns.insert(add_ai_entity(rand_pos));
			}
			else {
				a2e_error("no valid spawn position found for spawner @%v!", spwn->position);
			}
		}
	}
	
	update_dynamic_render_data();
}

void sb_map::update(const unsigned int& chunk_index, const uint3& local_position, const BLOCK_MATERIAL& mat) {
	const unsigned int block_idx = block_position_to_index(local_position);
	const BLOCK_MATERIAL& old_mat(chunks[chunk_index][block_idx].material);
	const uint3 position(chunk_extent * chunk_index_to_position(chunk_index) + local_position);
	const float3 center_position(float3(position) + 0.5f);
	
#if defined(DEBUG)
	if(block_idx >= blocks_per_chunk) {
		a2e_error("invalid block index %u - should be within %u!", block_idx, blocks_per_chunk);
		return;
	}
	if(chunk_index >= (chunk_count.x*chunk_count.y*chunk_count.z)) {
		a2e_error("invalid chunk index %u - should be within %u!", chunk_index, chunk_count.x*chunk_count.y*chunk_count.z);
		return;
	}
	if((unsigned int)mat >= (unsigned int)BLOCK_MATERIAL::__MAX_BLOCK_MATERIAL) {
		a2e_error("invalid material %u!", mat);
		return;
	}
#endif
	
	// handle physics blocks (must be handled before adding the event):
	if(old_mat != BLOCK_MATERIAL::NONE &&
	   mat == BLOCK_MATERIAL::NONE &&
	   static_bodies[chunk_index].count(block_idx) > 0) {
		// remove body
		rigid_body* body = static_bodies[chunk_index][block_idx];
		pc->remove_rigid_body(body);
		static_bodies[chunk_index].erase(block_idx);
	}
	else if((old_mat == BLOCK_MATERIAL::NONE || old_mat == BLOCK_MATERIAL::ACID) &&
			mat != BLOCK_MATERIAL::NONE &&
			mat != BLOCK_MATERIAL::ACID &&
			static_bodies[chunk_index].count(block_idx) == 0) {
		// add body
		static_bodies[chunk_index].insert(make_pair(block_idx, &pc->add_rigid_body(*block_rinfo, center_position)));
	}
	
	// add event
	eevt->add_event(EVENT_TYPE::BLOCK_CHANGE, make_shared<block_change_event>(SDL_GetTicks(), chunk_index, block_idx, old_mat, mat));
	
	// handle light blocks:
	if(old_mat != BLOCK_MATERIAL::LIGHT &&
	   mat == BLOCK_MATERIAL::LIGHT &&
	   lights[chunk_index].count(block_idx) == 0) {
		// add light
		light* l = new light(center_position);
		l->set_radius(16.0f);
		l->set_color(compute_light_color_for_position(position));
		sce->add_light(l);
		lights[chunk_index].insert(make_pair(block_idx, l));
	}
	else if(old_mat == BLOCK_MATERIAL::LIGHT &&
			mat != BLOCK_MATERIAL::LIGHT &&
			lights[chunk_index].count(block_idx) > 0) {
		// remove light
		light* l = lights[chunk_index][block_idx];
		sce->delete_light(l);
		delete l;
		lights[chunk_index].erase(block_idx);
	}
	
	// handle spawner blocks:
	if(old_mat != BLOCK_MATERIAL::SPAWNER && mat == BLOCK_MATERIAL::SPAWNER) {
		// add spawner
		add_spawner(position);
	}
	else if(old_mat == BLOCK_MATERIAL::SPAWNER && mat != BLOCK_MATERIAL::SPAWNER) {
		// remove spawner
		remove_spawner(position);
	}
	
	// recompute light intensity for light triggers
	if(old_mat == BLOCK_MATERIAL::LIGHT || mat == BLOCK_MATERIAL::LIGHT) {
		for(const auto& trgr : triggers) {
			if(trgr->type != TRIGGER_TYPE::LIGHT) continue;
			
			const float intensity = light_intensity_for_position(trgr->position);
			if(trgr->state.active && intensity < trgr->intensity) {
				trgr->deactivate(this);
			}
			if(!trgr->state.active && intensity >= trgr->intensity) {
				trgr->activate(this);
			}
		}
	}
	
	// and finally: update data
	chunks[chunk_index][block_idx].material = mat;
	const int3 max_extent(chunk_count * chunk_extent);
	
	// update render chunks data
	const auto accessor = [this, &max_extent](const int3& global_pos) -> BLOCK_MATERIAL {
		// check if this is an outside position
		if(global_pos.x < 0 || global_pos.y < 0 || global_pos.z < 0 ||
		   global_pos.x >= max_extent.x || global_pos.y >= max_extent.y || global_pos.z >= max_extent.z) {
			// if so, pretend there is a block, so all chunk outside blocks/sides get culled
			return BLOCK_MATERIAL::INDESTRUCTIBLE;
		}
		
		const uint3 local_pos(global_pos % chunk_extent);
		const uint3 chunk_pos(global_pos / chunk_extent);
		const unsigned int block(sb_map::block_position_to_index(local_pos));
		const unsigned int chunk(chunk_position_to_index(chunk_pos));
		return chunks[chunk][block].material;
	};
	const auto update_culling_data = [this, &accessor, &max_extent](const int3& pos) {
		if(pos.x < 0 || pos.y < 0 || pos.z < 0) return;
		if(pos.x >= max_extent.x || pos.y >= max_extent.y || pos.z >= max_extent.z) return;
		
		const unsigned int index(block_position_to_index(pos % chunk_extent));
		unsigned int block_mat = (unsigned int)accessor(pos);
		unsigned int culling_data = 0;
		{
			// bottom
			culling_data |= (accessor(int3(pos.x, pos.y - 1, pos.z)) == BLOCK_MATERIAL::NONE ?
							 (unsigned int)BLOCK_FACE::INVALID : (unsigned int)BLOCK_FACE::BOTTOM);
			
			// top
			culling_data |= (accessor(int3(pos.x, pos.y + 1, pos.z)) == BLOCK_MATERIAL::NONE ?
							 (unsigned int)BLOCK_FACE::INVALID : (unsigned int)BLOCK_FACE::TOP);
			
			// front
			culling_data |= (accessor(int3(pos.x, pos.y, pos.z - 1)) == BLOCK_MATERIAL::NONE ?
							 (unsigned int)BLOCK_FACE::INVALID : (unsigned int)BLOCK_FACE::FRONT);
			
			// back
			culling_data |= (accessor(int3(pos.x, pos.y, pos.z + 1)) == BLOCK_MATERIAL::NONE ?
							 (unsigned int)BLOCK_FACE::INVALID : (unsigned int)BLOCK_FACE::BACK);
			
			// right
			culling_data |= (accessor(int3(pos.x + 1, pos.y, pos.z)) == BLOCK_MATERIAL::NONE ?
							 (unsigned int)BLOCK_FACE::INVALID : (unsigned int)BLOCK_FACE::RIGHT);
			
			// left
			culling_data |= (accessor(int3(pos.x - 1, pos.y, pos.z)) == BLOCK_MATERIAL::NONE ?
							 (unsigned int)BLOCK_FACE::INVALID : (unsigned int)BLOCK_FACE::LEFT);
		}
		
		// flag if material texture should be flipped horizontally every other y layer
		static const vector<bool> flip_mat {
			{
				false,	// NONE,
				true,	// INDESTRUCTIBLE,
				false,	// METAL,
				false,	// __PLACEHOLDER_0,
				false,	// MAGNET,
				false,	// LIGHT,
				false,	// __PLACEHOLDER_1,
				false,	// ACID,
				false,	// __PLACEHOLDER_2,
				false,	// __PLACEHOLDER_3,
				false,	// SPRING,
				false,	// SPAWNER,
				false,	// __MAX_BLOCK_MATERIAL
			}
		};
		const bool flip_material = flip_mat[block_mat];
		block_mat = remap_material((BLOCK_MATERIAL)block_mat);
		if(flip_material) block_mat |= 0x8000;
		
		//
		const unsigned int render_data = block_mat + (culling_data << 16);
		glBindBuffer(GL_UNIFORM_BUFFER, render_chunks[chunk_position_to_index(pos / chunk_extent)].ubo);
		glBufferSubData(GL_UNIFORM_BUFFER, index * sizeof(unsigned int), sizeof(unsigned int), &render_data);
	};
	
	const int3 global_position(chunk_index_to_position(chunk_index) * chunk_extent + local_position);
	update_culling_data(global_position);
	
	// also update all neighboring blocks
	update_culling_data(int3(global_position.x - 1, global_position.y, global_position.z));
	update_culling_data(int3(global_position.x + 1, global_position.y, global_position.z));
	update_culling_data(int3(global_position.x, global_position.y - 1, global_position.z));
	update_culling_data(int3(global_position.x, global_position.y + 1, global_position.z));
	update_culling_data(int3(global_position.x, global_position.y, global_position.z - 1));
	update_culling_data(int3(global_position.x, global_position.y, global_position.z + 1));
	
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

float sb_map::light_intensity_for_position(const uint3& global_position) const {
	float intensity = 0.0f;
	const float3 pos(float3(global_position) + 0.5f);
	for(const auto& chunk_lights : lights) {
		for(const auto& block_light : chunk_lights) {
			const light* li(block_light.second);
			
			// attenuation = distance / light_radius^4
			const float3 light_dir((li->get_position() - pos) * li->get_inv_sqr_radius());
			const float attenuation(1.0f - light_dir.dot(light_dir) * li->get_sqr_radius());
			if(attenuation > 0.0f) {
				intensity += li->get_radius() * attenuation;
			}
		}
	}
	return intensity;
}

void sb_map::update(const uint3& global_position, const BLOCK_MATERIAL& mat) {
	const uint3 chunk_position(global_position / chunk_extent);
	const uint3 local_position(global_position - (chunk_position * chunk_extent));
	update(chunk_position, local_position, mat);
}

void sb_map::update(const pair<uint3, uint3>& global_min_max_position, const BLOCK_MATERIAL& mat) {
	for(unsigned int py = global_min_max_position.first.y; py < global_min_max_position.second.y+1; py++) {
		for(unsigned int pz = global_min_max_position.first.z; pz < global_min_max_position.second.z+1; pz++) {
			for(unsigned int px = global_min_max_position.first.x; px < global_min_max_position.second.x+1; px++) {
				update(uint3(px, py, pz), mat);
			}
		}
	}
}

void sb_map::update(const uint3& chunk_position, const uint3& local_position, const BLOCK_MATERIAL& mat) {
	const unsigned int chunk_idx = chunk_position_to_index(chunk_position);
	update(chunk_idx, local_position, mat);
}

void sb_map::update(const chunk& chnk, const uint3& local_position, const BLOCK_MATERIAL& mat) {
	// check if the chunk exists and get its index
	ssize_t chunk_idx = -1, index_counter = 0;
	for(auto iter = chunks.cbegin(); iter != chunks.cend(); index_counter++, iter++) {
		if(&chnk == &*iter) {
			chunk_idx = index_counter;
			break;
		}
	}
	if(chunk_idx != -1) {
		a2e_error("invalid chunk update (0x%X, #%u, %u)!", &chnk, chunk_idx, mat);
		return;
	}
	
	update((unsigned int)chunk_idx, local_position, mat);
}

const vector<sb_map::chunk>& sb_map::get_chunks() const {
	return chunks;
}

const sb_map::chunk& sb_map::get_chunk(const unsigned int& chunk_index) const {
	return chunks[chunk_index];
}

const sb_map::block_data& sb_map::get_block(const unsigned int& chunk_index, const unsigned int& block_index) const {
	return chunks[chunk_index][block_index];
}

const sb_map::block_data& sb_map::get_block(const uint3& global_position) const {
	const unsigned int chunk_index(chunk_position_to_index(global_position / chunk_extent));
	const unsigned int block_index(sb_map::block_position_to_index(global_position % chunk_extent));
	return chunks[chunk_index][block_index];
}

void sb_map::resize(const uint3& chunk_count_) {
	pc->lock();
	
	// on the first call to this function, chunk_count is (0,0,0)
	// -> on live resizing (a second+ call) this must/will be != 0
	const bool live_resize = (chunk_count.x != 0);
	const uint3 old_chunk_count = chunk_count;
	vector<chunk> old_chunks;
	vector<unordered_map<unsigned int, rigid_body*>> old_static_bodies;
	vector<unordered_map<unsigned int, rigid_body*>> old_dynamic_body_field;
	vector<unordered_map<unsigned int, light*>> old_lights;
	vector<int> chunk_remapping; // old index -> new index (-1: delete data)
	if(live_resize) {
		chunks.swap(old_chunks);
		static_bodies.swap(old_static_bodies);
		dynamic_body_field.swap(old_dynamic_body_field);
		lights.swap(old_lights);
		chunks.clear();
		static_bodies.clear();
		dynamic_body_field.clear();
		lights.clear();
		render_chunks.clear();
		
		chunk_remapping.resize(old_chunks.size(), -1);
		unsigned int old_index = 0;
		const unsigned int xz_size = chunk_count_.x * chunk_count_.z, x_size = chunk_count_.x;
		for(unsigned int cy = 0; cy < chunk_count.y; cy++) {
			for(unsigned int cz = 0; cz < chunk_count.z; cz++) {
				for(unsigned int cx = 0; cx < chunk_count.x; cx++, old_index++) {
					if(cx >= chunk_count_.x || cy >= chunk_count_.y || cz >= chunk_count_.z) {
						// smaller than new chunk count -> delete
						chunk_remapping[old_index] = -1;
						continue;
					}
					else {
						// -> remap
						chunk_remapping[old_index] = cy * xz_size + cz * x_size + cx;
					}
				}
			}
		}
	}
	
	chunk_count = chunk_count_;
	const size_t total_chunk_count = chunk_count.x * chunk_count.y * chunk_count.z;
	chunks.resize(total_chunk_count);
	static_bodies.resize(total_chunk_count);
	dynamic_body_field.resize(total_chunk_count);
	lights.resize(total_chunk_count);
	
	// copy old data into new containers
	if(live_resize) {
		for(unsigned int chunk_index = 0; chunk_index < chunk_remapping.size(); chunk_index++) {
			if(chunk_remapping[chunk_index] == -1) {
				// remove/delete all data for this chunk
				for(const auto& sbody : old_static_bodies[chunk_index]) {
					pc->remove_rigid_body(sbody.second);
				}
				old_static_bodies[chunk_index].clear();
				old_dynamic_body_field[chunk_index].clear(); // note: already deleted by static_bodies
				
				for(const auto& li : old_lights[chunk_index]) {
					sce->delete_light(li.second);
					delete li.second;
				}
				old_lights[chunk_index].clear();
				continue;
			}
			
			// copy/move
			chunks[chunk_remapping[chunk_index]].swap(old_chunks[chunk_index]);
			static_bodies[chunk_remapping[chunk_index]].swap(old_static_bodies[chunk_index]);
			dynamic_body_field[chunk_remapping[chunk_index]].swap(old_dynamic_body_field[chunk_index]);
			lights[chunk_remapping[chunk_index]].swap(old_lights[chunk_index]);
		}
	}
	
	// render data (at the moment just empty data ...)
	size_t chunk_counter = 0;
	for(const auto& chunk : chunks) {
		array<unsigned int, blocks_per_chunk> block_render_data;
		for(size_t block_idx = 0; block_idx < blocks_per_chunk; block_idx++) {
			block_render_data[block_idx] = remap_material(chunk[block_idx].material);
		}
		render_chunks.emplace_back(float3(chunk_counter % chunk_count.x,
										  chunk_counter / (chunk_count.x * chunk_count.z),
										  (chunk_counter / chunk_count.x) % chunk_count.z) * float(chunk_extent),
								   block_render_data);
		chunk_counter++;
	}
	
	// for convenience, initialize the lowest layer of new chunks (@y=0) with indestructible blocks
	if(live_resize) {
		for(unsigned int cz = 0; cz < chunk_count.z; cz++) {
			for(unsigned int cx = 0; cx < chunk_count.x; cx++) {
				if(cz < old_chunk_count.z && cx < old_chunk_count.x) continue;
				
				//
				//update(const unsigned int& chunk_index, const uint3& local_position, const BLOCK_MATERIAL& mat);
				uint3 local_position;
				for(unsigned int bz = 0; bz < chunk_extent; bz++) {
					local_position.z = bz;
					for(unsigned int bx = 0; bx < chunk_extent; bx++) {
						local_position.x = bx;
						update(cz * chunk_count.x + cx, local_position, BLOCK_MATERIAL::INDESTRUCTIBLE);
					}
				}
			}
		}
	}
	
	//
	pc->unlock();
}

const vector<sb_map::chunk_render_data>& sb_map::get_render_chunks() const {
	return render_chunks;
}

void sb_map::set_name(const string& name_) {
	name = name_;
}

const string& sb_map::get_name() const {
	return name;
}

void sb_map::set_filename(const string& filename_) {
	filename = filename_;
}

const string& sb_map::get_filename() const {
	return filename;
}

const uint3& sb_map::get_chunk_count() const {
	return chunk_count;
}

void sb_map::set_player_start(const uint3& player_start_) {
	player_start = player_start_;
	ge->set_position(player_start);
}

const uint3& sb_map::get_player_start() const {
	return player_start;
}

void sb_map::set_player_rotation(const float3& player_rotation_) {
	if((player_rotation == player_rotation_).all()) return;
	player_rotation = player_rotation_;
	const float angle = fabsf((player_rotation.x < 0.0f ? 360.0f : 0.0f) -
							  RAD2DEG(float3(0.0f, 0.0f, -1.0f).angle(player_rotation.normalized())));
	cam->set_rotation(0.0f, angle, 0.0f);
	ge->set_rotation(float3(0.0f, angle, 0.0f));
}

const float3& sb_map::get_player_rotation() const {
	return player_rotation;
}

rigid_body* sb_map::make_dynamic(const unsigned int& chunk_index, const unsigned int& block_index) {
	if(static_bodies[chunk_index].count(block_index) == 0) {
		a2e_error("there is no rigid body @%u:%u!", chunk_index, block_index);
		return nullptr;
	}
	
	switch(chunks[chunk_index][block_index].material) {
		case BLOCK_MATERIAL::INDESTRUCTIBLE:
		case BLOCK_MATERIAL::ACID:
		case BLOCK_MATERIAL::LIGHT:
		case BLOCK_MATERIAL::MAGNET:
			 return nullptr;
		default: break;
	}
	
	rigid_body* body = static_bodies[chunk_index][block_index];
	
	// remove from static bodies list (and add to dynamic one), before we update all data
	static_bodies[chunk_index].erase(block_index);
	dynamic_bodies.insert(make_pair(body, chunks[chunk_index][block_index].material));
	update(chunk_index, sb_map::block_index_to_position(block_index), BLOCK_MATERIAL::NONE);
	body->get_body()->setActivationState(DISABLE_DEACTIVATION);
	body->get_body()->setSleepingThresholds(0.0f, 0.0f);
	
	pc->make_dynamic(*body, 100.0f);
	
	return body;
}

rigid_body* sb_map::resolve_dynamic(const uint3& global_position) {
	const uint3 chunk_position(global_position / chunk_extent);
	const uint3 local_position(global_position - (chunk_position * chunk_extent));
	
	const unsigned int chunk_index = chunk_position_to_index(chunk_position);
	const unsigned int block_index = block_position_to_index(local_position);
		
	if(static_bodies[chunk_index].count(block_index) == 0 &&
	   dynamic_body_field[chunk_index].count(block_index) != 0) {
		// return the previously remembered dynamic body
		return dynamic_body_field[chunk_index][block_index];
	} else {
		rigid_body* body = make_dynamic(chunk_index, block_index);
		if(body == nullptr) return nullptr;
		dynamic_body_field[chunk_index].insert(make_pair(block_index, body));
		return body;
	}
}

const vector<ai_entity*>& sb_map::get_ai_entities() const {
	return entities;
}

void sb_map::update_dynamic_render_data() {
	if(dynamic_bodies.empty()) return;
	
	dynamic_render_data_size = 0;
	for(const auto& body : dynamic_bodies) {
		if(body.second == BLOCK_MATERIAL::NONE ||
		   body.second == BLOCK_MATERIAL::__PLACEHOLDER_0 ||
		   body.second == BLOCK_MATERIAL::__PLACEHOLDER_1 ||
		   body.second == BLOCK_MATERIAL::__PLACEHOLDER_2 ||
		   body.second == BLOCK_MATERIAL::__PLACEHOLDER_3) {
			continue;
		}
		matrix4f& mat(dynamic_render_data[dynamic_render_data_size++]);
		const btTransform& transform(body.first->get_body()->getWorldTransform());
		const btMatrix3x3& basis(transform.getBasis());
		const btVector3& origin(transform.getOrigin());
		const float3& scale(body.first->get_scale());
		
		mat[0] = basis[0][0];
		mat[1] = basis[1][0];
		mat[2] = basis[2][0];
		mat[3] = 0.0f;
		
		mat[4] = basis[0][1];
		mat[5] = basis[1][1];
		mat[6] = basis[2][1];
		mat[7] = 0.0f;
		
		mat[8] = basis[0][2];
		mat[9] = basis[1][2];
		mat[10] = basis[2][2];
		mat[11] = 0.0f;
		
		mat[12] = 0.0f;
		mat[13] = 0.0f;
		mat[14] = 0.0f;
		mat[15] = 1.0f;
		
		// to prevent z-fighting, slightly scale block sized dynamic objects
		if(scale.x == 1.0f && scale.y == 1.0f && scale.z == 1.0f) {
			mat.scale(1.005f, 1.005f, 1.005f);
		}
		else mat.scale(scale.x, scale.y, scale.z);
		
		mat[12] = origin.x();
		mat[13] = origin.y();
		mat[14] = origin.z();
		
		mat[15] = (float)remap_material(body.second);
	}
	
	glBindBuffer(GL_UNIFORM_BUFFER, dynamic_bodies_ubo);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, dynamic_render_data_size * sizeof(matrix4f), &dynamic_render_data[0]);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

const pair<GLuint, size_t> sb_map::get_dynamic_render_data() const {
	return make_pair(dynamic_bodies_ubo, dynamic_render_data_size);
}

const unordered_map<rigid_body*, BLOCK_MATERIAL>& sb_map::get_dynamic_bodies() const {
	return dynamic_bodies;
}

void sb_map::update_dynamic(rigid_body* body, const BLOCK_MATERIAL& block_material) {
	auto it = dynamic_bodies.find(body);
	if(it != dynamic_bodies.end()) {
		dynamic_bodies[body] = block_material;
	}
}

bool sb_map::event_handler(EVENT_TYPE type, shared_ptr<event_object> obj) {
	static bool player_step_side = false, ai_step_side = false;
	if (type == EVENT_TYPE::PLAYER_STEP) {
		const shared_ptr<player_step_event>& step_evt = (shared_ptr<player_step_event>&)obj;
		play_sound(player_step_side ? "WALK_R" : "WALK_L", step_evt->position);
		player_step_side ^= true;
		save->inc_step_count();
		return true;
	} else if (type == EVENT_TYPE::AI_STEP) {
		const shared_ptr<ai_step_event>& step_evt = (shared_ptr<ai_step_event>&)obj;
		play_sound(ai_step_side ? "WALK_AI_R" : "WALK_AI_L", step_evt->position);
		ai_step_side ^= true;
		return true;
	} else if (type == EVENT_TYPE::PLAYER_BLOCK_STEP || type == EVENT_TYPE::AI_BLOCK_STEP) {
		uint3 entity_pos;
		physics_entity* entity = nullptr;
		if (type == EVENT_TYPE::PLAYER_BLOCK_STEP) {
			const shared_ptr<player_block_step_event>& step_evt = (shared_ptr<player_block_step_event>&)obj;
			entity_pos = step_evt->block;
			entity = ge;
		} else if (type == EVENT_TYPE::AI_BLOCK_STEP) {
			const shared_ptr<ai_block_step_event>& step_evt = (shared_ptr<ai_block_step_event>&)obj;
			entity_pos = step_evt->block;
			entity = (physics_entity*)step_evt->ai;
		} else {
			return false;
		}
		
		// get blocks in a 5x7x5 box surrounding the player
		// .....
		// .....
		// ..x..
		// ..X..
		// ..X..
		// .....
		// .....
		static const int3 entity_offset(2, 2, 2);
		static array<array<array<block_data, 5>, 7>, 5> entity_blocks;
		const int3 ientity_pos(entity_pos);
		for(size_t x = 0; x < entity_blocks.size(); x++) {
			for(size_t y = 0; y < entity_blocks[x].size(); y++) {
				for(size_t z = 0; z < entity_blocks[x][y].size(); z++) {
					const int3 pos(ientity_pos - entity_offset + int3(int(x), int(y), int(z)));
					if(is_valid_position(pos)) {
						entity_blocks[x][y][z].material = get_block(pos).material;
					}
					else entity_blocks[x][y][z].material = BLOCK_MATERIAL::NONE;
				}
			}
		}
		
		// check block type of the block the player is currently in (note: only checks the lowest player block!)
		const BLOCK_MATERIAL mat(entity_blocks[entity_offset.x][entity_offset.y][entity_offset.z].material);
		if(entity == ge) {
			switch(mat) {
				case BLOCK_MATERIAL::ACID:
					play_sound("SPLASH", entity_pos, 1.0f, false, false);
					kill_player();
					break;
				default: break;
			}
			
			// check for map change
			for(const auto& ml : map_links) {
				if(!ml->enabled) continue;
				for(const auto& pos : ml->positions) {
					if(pos.x == entity_pos.x && pos.y == entity_pos.y && pos.z == entity_pos.z) {
						map_change = { true, ml };
						break;
					}
				}
				if(map_change.first) break;
			}
		} else {
			switch(mat) {
				case BLOCK_MATERIAL::ACID:
					play_sound("SPLASH", entity_pos);
					break;
				default: break;
			}
		}
		
		// check block type of the block the player stands on
		if(entity_pos.y > 0) {
			const BLOCK_MATERIAL below_mat(entity_blocks[entity_offset.x][entity_offset.y - 1][entity_offset.z].material);
			switch(below_mat) {
				default:
					break;
			}
		}
		
		// magnet check
		static const array<int3, 14> magnet_offsets {
			{
				int3(-1, 0, 0), int3(1, 0, 0), int3(0, 0, -1), int3(0, 0, 1),
				int3(-1, 1, 0), int3(1, 1, 0), int3(0, 1, -1), int3(0, 1, 1),
				int3(-1, 2, 0), int3(1, 2, 0), int3(0, 2, -1), int3(0, 2, 1),
				int3(0, -1, 0), int3(0, 3, 0),
			}
		};
		entity->reset_speed();
		entity->reset_jump_strength();
		for(const auto& magnet_offset : magnet_offsets) {
			if(entity_blocks[magnet_offset.x + entity_offset.x][magnet_offset.y + entity_offset.y][magnet_offset.z + entity_offset.z].material == BLOCK_MATERIAL::MAGNET) {
				entity->set_speed(1.0f);
				entity->set_jump_strength(1.0f);
				if(entity == ge) play_sound("MAGNET", ientity_pos + magnet_offset);
			}
		}
		
		// handling spring blocks is slightly more complicated (blocks in a 5x7x5 box must be checked)
		static const array<int3, 28> spring_block_offsets {
			{
				int3(-2, 0, 0), int3(-1, 0, 0), int3(1, 0, 0), int3(2, 0, 0),
				int3(0, 0, -2), int3(0, 0, -1), int3(0, 0, 1), int3(0, 0, 2),
				int3(-2, 1, 0), int3(-1, 1, 0), int3(1, 1, 0), int3(2, 1, 0),
				int3(0, 1, -2), int3(0, 1, -1), int3(0, 1, 1), int3(0, 1, 2),
				int3(-2, 2, 0), int3(-1, 2, 0), int3(1, 2, 0), int3(2, 2, 0),
				int3(0, 2, -2), int3(0, 2, -1), int3(0, 2, 1), int3(0, 2, 2),
				int3(0, -2, 0), int3(0, -1, 0), int3(0, 1, 0), int3(0, 2, 0),
			}
		};
		const int3 bounds(chunk_count * chunk_extent);
		for(const auto& spring_block : spring_block_offsets) {
			if(entity_blocks[spring_block.x + entity_offset.x][spring_block.y + entity_offset.y][spring_block.z + entity_offset.z].material == BLOCK_MATERIAL::SPRING) {
				const int3 spring_pos(entity_pos + spring_block);
				const int3 spring_dir(-((spring_block.x == 0 && spring_block.z == 0) ?
										int3(0, spring_block.y > 0 ? 1 : -1, 0) :
										((spring_block.x == 0) ? int3(0, 0, spring_block.z > 0 ? 1 : -1) :
										 int3(spring_block.x > 0 ? 1 : -1, 0, 0))));
				// check if spring can extend into this direction (-> 2 NONE blocks)
				if(is_valid_position(spring_pos + spring_dir) && is_valid_position(spring_pos + spring_dir * 2) &&
				   get_block(spring_pos + spring_dir).material == BLOCK_MATERIAL::NONE &&
				   get_block(spring_pos + spring_dir * 2).material == BLOCK_MATERIAL::NONE) {
					add_spring(spring_pos, spring_dir);
				}
			}
		}
		
		return true;
	}
	return false;
}

void sb_map::add_spring(const uint3& position, const float3& direction) {
	// check if a spring for that position is already active
	for(const auto& sp : springs) {
		if((sp->position == position).all()) return;
	}
	
	//
	rigid_info* sp_info = &pc->add_rigid_info<physics_controller::SHAPE::BOX>(0.0f, float3(0.5f));
	rigid_body* sp = &pc->add_rigid_body(*sp_info, float3(position) + 0.5f);
	dynamic_bodies.insert(make_pair(sp, BLOCK_MATERIAL::SPRING));
	springs.emplace_back(new spring {
		position,
		direction,
		sp_info,
		sp,
		0.0f,
		1.0f,
		SDL_GetTicks(),
	});
	
	play_sound("SPRING", position);
}

void sb_map::set_background_music(audio_background* bg) {
	background_music = bg;
}

audio_background* sb_map::get_background_music() const {
	return background_music;
}

void sb_map::remove_background_music() {
	ac->delete_audio_source(background_music->get_identifier());
	background_music = nullptr;
}

void sb_map::add_sound(audio_3d* sound) {
	env_sounds.insert(sound);
}

const set<audio_3d*>& sb_map::get_sounds() const {
	return env_sounds;
}

audio_3d* sb_map::get_sound(const string& identifier) const {
	for(const auto& snd : env_sounds) {
		if(snd->get_identifier() == identifier) {
			return snd;
		}
	}
	return nullptr;
}

void sb_map::remove_sound(audio_3d* sound) {
	const auto iter = find(begin(env_sounds), end(env_sounds), sound);
	if(iter != end(env_sounds)) {
		const string identifier(sound->get_identifier());
		env_sounds.erase(iter);
		if(!ac->delete_audio_source(identifier)) {
			a2e_error("failed to delete audio source \"%s\"!", identifier);
		}
	}
}

light* sb_map::get_light(const uint3& global_position) const {
	const unsigned int chunk_index(chunk_position_to_index(global_position / chunk_extent));
	const unsigned int block_index(sb_map::block_position_to_index(global_position % chunk_extent));
	if(lights[chunk_index].count(block_index) == 0) {
		return nullptr;
	}
	return lights[chunk_index].at(block_index);
}

const vector<unordered_map<unsigned int, light*>>& sb_map::get_lights() const {
	return lights;
}

void sb_map::add_map_link(map_link* ml) {
	map_links.push_back(ml);
}

void sb_map::remove_map_link(map_link* ml) {
	const auto iter = find(begin(map_links), end(map_links), ml);
	if(iter != end(map_links)) {
		map_links.erase(iter);
		delete ml;
	}
}

const pair<bool, sb_map::map_link*>& sb_map::is_map_change() const {
	return map_change;
}

const vector<sb_map::map_link*>& sb_map::get_map_links() const {
	return map_links;
}

sb_map::map_link* sb_map::get_map_link(const string& identifier) const {
	for(const auto& ml : map_links) {
		if(ml->identifier == identifier) {
			return ml;
		}
	}
	return nullptr;
}

void sb_map::add_ai_waypoint(sb_map::ai_waypoint* wp) {
	ai_waypoints.push_back(wp);
}

void sb_map::remove_ai_waypoint(sb_map::ai_waypoint* wp) {
	const auto iter = find(begin(ai_waypoints), end(ai_waypoints), wp);
	if(iter == ai_waypoints.end()) return;
	ai_waypoints.erase(iter);
	delete wp;
}

const vector<sb_map::ai_waypoint*>& sb_map::get_ai_waypoints() const {
	return ai_waypoints;
}

sb_map::ai_waypoint* sb_map::get_ai_waypoint(const string& identifier) const {
	for(const auto& wp : ai_waypoints) {
		if(wp->identifier == identifier) {
			return wp;
		}
	}
	return nullptr;
}

ai_entity* sb_map::add_ai_entity(const uint3& position) {
	if(((position / (unsigned int)chunk_extent) >= chunk_count).any()) {
		a2e_error("invalid position: %v!", position);
		return nullptr;
	}
	ai_entity* entity = new ai_entity(float3(position) + 0.5f);
	entities.push_back(entity);
	return entity;
}

bool sb_map::is_valid_position(const uint3& global_position) const {
	const uint3 max_pos(chunk_extent * chunk_count);
	if(global_position.x >= max_pos.x || global_position.y >= max_pos.y || global_position.z >= max_pos.z) {
		return false;
	}
	return true;
}

void sb_map::add_trigger(sb_map::trigger* trgr) {
	triggers.emplace_back(trgr);
	if(!trgr->on_load.empty()) {
		trgr->state.active = true;
		if(trgr->sub_type == TRIGGER_SUB_TYPE::TIMED) {
			trgr->state.timer = trgr->time + SDL_GetTicks();
		}
		trgr->scr->execute(this, trgr->on_load, trgr->position);
	}
	
	switch(trgr->type) {
		case TRIGGER_TYPE::WEIGHT: {
			trgr->state.slider = pc->add_weight_slider(trgr->position, 0.25f, trgr->weight);
		}
		break;
		default: break;
	}
}

void sb_map::remove_trigger(sb_map::trigger* trgr) {
	const auto iter = find(begin(triggers), end(triggers), trgr);
	if(iter != end(triggers)) {
		triggers.erase(iter);
		delete trgr;
	}
}

const vector<sb_map::trigger*> sb_map::get_triggers() const {
	return triggers;
}

sb_map::trigger* sb_map::get_trigger(const string& identifier) const {
	for(const auto& trgr : triggers) {
		if(trgr->identifier == identifier) {
			return trgr;
		}
	}
	return nullptr;
}

void sb_map::handle_block_click(const pair<uint3, BLOCK_FACE>& clicked_block, bool& activated) {
	// check push button triggers
	for(auto& trgr : triggers) {
		if(trgr->type == TRIGGER_TYPE::PUSH &&
		   ((unsigned int)clicked_block.second & (unsigned int)trgr->facing) != 0 &&
		   (clicked_block.first == trgr->position).all()) {
			trgr->state.active ? trgr->deactivate(this) : trgr->activate(this);
			activated = true;
		}
	}
}

void sb_map::add_spawner(const uint3& position) {
	spawners.emplace_back(new spawner {
		position,
		1,
		set<ai_entity*> {}
	});
}

void sb_map::remove_spawner(const uint3& position) {
	for(auto iter = begin(spawners); iter != end(spawners); iter++) {
		if(((*iter)->position == position).all()) {
			spawners.erase(iter);
			break;
		}
	}
}

audio_3d* sb_map::play_sound(const string& identifier, const float3& position, const float volume, const bool is_looping, const bool can_be_killed) const {
	audio_3d* src = ac->add_audio_3d(identifier, "map."+size_t2string(SDL_GetPerformanceCounter()), true);
	src->set_position(position);
	src->set_volume(volume);
	src->set_can_be_killed(can_be_killed);
	if(is_looping) {
		src->play();
		src->loop(true);
	}
	else {
		src->play();
	}
	return src;
}

void sb_map::stop_sound(const string& identifier) const {
	audio_source* src = ac->get_audio_source(identifier);
	if(src != nullptr) {
		src->stop();
	}
}

void sb_map::kill_player() {
	ac->kill_audio_sources();
	map_change = { true, nullptr };
}

void sb_map::change_map(const string map_filename, const GAME_STATUS status) /* don't ref this */ {
	// use this function to load or change maps! (+extend if necessary)
	e->acquire_gl_context();
	if(!ge->is_enabled()) {
		ge->set_enabled(true);
	}
	ge->set_status(status);
	if(active_map != nullptr) {
		active_map->run(); // must be run to render scene correctly
	}
	cam->run(); // must be run to render scene correctly
	e->stop_draw(); // this will render the ui while the new map is being loaded
	e->start_draw();
	
	pc->lock();
	
	// always send a map unload event before we kill the map
	if(active_map != nullptr) {
		eevt->add_event(EVENT_TYPE::MAP_UNLOAD, make_shared<map_unload_event>(SDL_GetTicks()));
		delete active_map;
		active_map = nullptr;
	}
	pc->halt_simulation();
	
	if(map_storage::load(map_filename) == nullptr) {
		a2e_error("failed to load map \"%s\"!", map_filename);
		active_map = nullptr;
	}
	else if(status == GAME_STATUS::MAP_LOAD) {
		save->set_map(map_filename);
	}
	pc->resume_simulation();
	
	pc->unlock();
	e->release_gl_context();
}

void sb_map::set_default_light_color(const float3& color) {
	default_light_color = color;
}

const float3& sb_map::get_default_light_color() const {
	return default_light_color;
}

void sb_map::add_light_color_area(sb_map::light_color_area* lca) {
	light_color_areas.push_back(lca);
}

void sb_map::remove_light_color_area(sb_map::light_color_area* lca) {
	const auto iter = find(begin(light_color_areas), end(light_color_areas), lca);
	if(iter == end(light_color_areas)) return;
	light_color_areas.erase(iter);
	delete lca;
}

const vector<sb_map::light_color_area*>& sb_map::get_light_color_areas() const {
	return light_color_areas;
}

float3 sb_map::compute_light_color_for_position(const uint3& global_position) const {
	const auto is_influenced_by_lca = [](const uint3& pos, const light_color_area& lca) -> bool {
		if(pos.x >= lca.min_pos.x && pos.x <= lca.max_pos.x &&
		   pos.y >= lca.min_pos.y && pos.y <= lca.max_pos.y &&
		   pos.z >= lca.min_pos.z && pos.z <= lca.max_pos.z) {
			return true;
		}
		return false;
	};
	
	unsigned int lca_count = 0;
	float3 light_color;
	for(const auto& lca : light_color_areas) {
		if(is_influenced_by_lca(global_position, *lca)) {
			light_color += lca->color;
			lca_count++;
		}
	}
	
	if(lca_count == 0) {
		// not influenced by any light color area -> use default map light color
		return default_light_color;
	}
	
	// lca colors are additivie -> clamp to [0, 1] and set light color
	light_color.clamp(0.0f, 1.0f);
	return light_color;
}

void sb_map::update_light_colors() {
	for(unsigned int chunk_idx = 0, chnk_count = (unsigned int)chunks.size(); chunk_idx < chnk_count; chunk_idx++) {
		for(const auto& li : lights[chunk_idx]) {
			li.second->set_color(compute_light_color_for_position(block_index_to_position(li.first) +
																  chunk_index_to_position(chunk_idx) * chunk_extent));
		}
	}
}

unsigned int sb_map::remap_material(const BLOCK_MATERIAL& mat) {
	// remap material
	static const vector<unsigned int> mat_remap {
		{
			0,	// NONE,
			1,	// INDESTRUCTIBLE,
			2,	// METAL,
			0,	// __PLACEHOLDER_0,
			3,	// MAGNET,
			4,	// LIGHT,
			0,	// __PLACEHOLDER_1,
			5,	// ACID,
			0,	// __PLACEHOLDER_2,
			0,	// __PLACEHOLDER_3,
			6,	// SPRING,
			7,	// SPAWNER,
			0,	// __MAX_BLOCK_MATERIAL
		}
	};
	return mat_remap[(unsigned int)mat];
}

////////////////////
// chunk_render_data

sb_map::chunk_render_data::chunk_render_data(const float3& offset_, array<unsigned int, blocks_per_chunk>& render_data) : offset(offset_), ubo(0) {
	// gen and init (with no culling info):
	glGenBuffers(1, &ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, ubo);
	glBufferData(GL_UNIFORM_BUFFER, blocks_per_chunk * sizeof(unsigned int), &render_data[0], GL_STATIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

sb_map::chunk_render_data::chunk_render_data(chunk_render_data&& crd) : offset(crd.offset), ubo(crd.ubo) {
	crd.ubo = 0;
}

sb_map::chunk_render_data::~chunk_render_data() {
	if(glIsBuffer(ubo)) glDeleteBuffers(1, &ubo);
}

////////////////////
// trigger

void sb_map::trigger::activate(sb_map* cur_map) {
	if(state.active) return;
	state.active = true;
	if(sub_type == TRIGGER_SUB_TYPE::TIMED) {
		state.timer = time + SDL_GetTicks();
	}
	if(!on_trigger.empty()) {
		scr->execute(cur_map, on_trigger, position);
	}
}
void sb_map::trigger::deactivate(sb_map* cur_map) {
	if(!state.active) return;
	if(sub_type == TRIGGER_SUB_TYPE::TIMED &&
	   (state.timer == 0 || state.timer >= SDL_GetTicks())) return;
	state.active = false;
	if(sub_type == TRIGGER_SUB_TYPE::TIMED) {
		state.timer = 0;
	}
	if(!on_untrigger.empty()) {
		scr->execute(cur_map, on_untrigger, position);
	}
}
