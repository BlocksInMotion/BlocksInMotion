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

#include "map_storage.h"
#include "physics_controller.h"
#include "sb_map.h"
#include "audio_controller.h"
#include "soft_body.h"
#include "script_handler.h"
#include "script.h"
#include "game.h"
#include <core/file_io.h>
#include <scene/scene.h>

#define SB_DATA_TYPE_TO_STR(type) string(string("") + \
										 (char)((type >> 24) & 0xFF) + \
										 (char)((type >> 16) & 0xFF) + \
										 (char)((type >> 8) & 0xFF) + \
										 (char)(type & 0xFF))

const unsigned int map_storage::header_magic = 'SBMP';
static const unsigned int uint_placeholder = 0xDEADBEEF;
static unordered_map<sb_map::ai_waypoint*, string> ai_waypoint_resolve_map;

const unordered_map<unsigned int, map_storage::load_function> map_storage::loaders {
	{ (unsigned int)map_storage::DATA_TYPES::MAP_DATA, &map_storage::load_map_data },
	{ (unsigned int)map_storage::DATA_TYPES::AUDIO_BACKGROUND, &map_storage::load_audio_background },
	{ (unsigned int)map_storage::DATA_TYPES::AUDIO_3D, &map_storage::load_audio_3d },
	{ (unsigned int)map_storage::DATA_TYPES::MAP_LINK, &map_storage::load_map_link },
	{ (unsigned int)map_storage::DATA_TYPES::TRIGGER, &map_storage::load_trigger },
	{ (unsigned int)map_storage::DATA_TYPES::LIGHT_COLOR_AREA, &map_storage::load_light_color_area },
	{ (unsigned int)map_storage::DATA_TYPES::AI_WAYPOINT, &map_storage::load_ai_waypoint },
};

const unordered_map<unsigned int, map_storage::save_function> map_storage::savers {
	{ (unsigned int)map_storage::DATA_TYPES::MAP_DATA, &map_storage::save_map_data },
	{ (unsigned int)map_storage::DATA_TYPES::AUDIO_BACKGROUND, &map_storage::save_audio_background },
	{ (unsigned int)map_storage::DATA_TYPES::AUDIO_3D, &map_storage::save_audio_3d },
	{ (unsigned int)map_storage::DATA_TYPES::MAP_LINK, &map_storage::save_map_link },
	{ (unsigned int)map_storage::DATA_TYPES::TRIGGER, &map_storage::save_trigger },
	{ (unsigned int)map_storage::DATA_TYPES::LIGHT_COLOR_AREA, &map_storage::save_light_color_area },
	{ (unsigned int)map_storage::DATA_TYPES::AI_WAYPOINT, &map_storage::save_ai_waypoint },
};

const unordered_map<unsigned int, unsigned int> map_storage::data_versions {
	{ (unsigned int)map_storage::DATA_TYPES::MAP_DATA, 2 },
	{ (unsigned int)map_storage::DATA_TYPES::AUDIO_BACKGROUND, 1 },
	{ (unsigned int)map_storage::DATA_TYPES::AUDIO_3D, 1 },
	{ (unsigned int)map_storage::DATA_TYPES::MAP_LINK, 2 },
	{ (unsigned int)map_storage::DATA_TYPES::TRIGGER, 2 },
	{ (unsigned int)map_storage::DATA_TYPES::LIGHT_COLOR_AREA, 1 },
	{ (unsigned int)map_storage::DATA_TYPES::AI_WAYPOINT, 1 },
};

sb_map* map_storage::load(const string& filename) {
	file_io file(e->data_path("maps/"+filename), file_io::OPEN_TYPE::READ_BINARY);
	if(!file.is_open()) {
		return nullptr;
	}
	
	// spec: http://albion2.org/mp/wiki/Level-Format (version #2)
	
	// magic and version check
	const unsigned int magic = file.get_uint();
	if(magic != header_magic) {
		a2e_error("invalid map header/magic: %X", magic);
		return nullptr;
	}
	
	const unsigned int map_version = file.get_uint();
	if(map_version != sb_map::map_version || file.fail()) {
		a2e_error("invalid map version %u (should be %u) in map %s!", map_version, sb_map::map_version, filename);
		return nullptr;
	}
	
	// create the map
	sb_map* level = new sb_map(filename);
	ai_waypoint_resolve_map.clear();
	
	try {
		// remaining header:
		string map_name = "";
		file.get_terminated_block(map_name, 0);
		level->set_name(map_name);
		a2e_debug(":: map name: %s", map_name);
		
		const unsigned int struct_count = file.get_uint();
		if(struct_count == 0) {
			throw a2e_exception("empty map");
		}
		
		for(unsigned int i = 0; i < struct_count; i++) {
			const unsigned int type = file.get_uint();
			const unsigned int version = file.get_uint();
			const unsigned int data_length = file.get_uint();
			
			// check if file is still valid
			if(file.fail()) {
				throw a2e_exception("read/extraction fail in struct #"+uint2string(i));
			}
			
			// check if we have a loader for this type
			if(loaders.count(type) == 0) {
				a2e_error("no loader for type '%s' (version: %u, length: %u)!",
						  SB_DATA_TYPE_TO_STR(type), version, data_length);
				
				// ignore struct and seek ahead
				file.seek((size_t)file.get_current_offset() + data_length);
				continue;
			}
			
			// check if the loader is for the correct version
			if(version != data_versions.at(type)) {
				a2e_error("invalid '%s' version: %u, should be %u!",
						  SB_DATA_TYPE_TO_STR(type), version, data_versions.at(type));
				// ignore struct and seek ahead
				file.seek((size_t)file.get_current_offset() + data_length);
				continue;
			}
			
			// load the data
			const long long int start_offset = file.get_current_offset();
			loaders.at(type)(file, *level);
			const long long int end_offset = file.get_current_offset();
			if((end_offset - start_offset) != data_length) { // check length
				throw a2e_exception("invalid '"+SB_DATA_TYPE_TO_STR(type)+"' length: expected length: "+
									uint2string(data_length)+", actual length: "+ssize_t2string(end_offset - start_offset));
			}
		}
		
		// resolve waypoints (this must be done after all waypoints have been loaded, since there might be cyclic dependencies)
		for(const auto& wp : ai_waypoint_resolve_map) {
			for(const auto& ai_wp : level->get_ai_waypoints()) {
				if(wp.second == ai_wp->identifier) {
					wp.first->next = ai_wp;
				}
			}
			
			//
			if(wp.first->next == nullptr && wp.second != "") {
				throw a2e_exception("couldn't find waypoint \""+wp.second+"\"");
			}
		}
		
		// after all light color areas have been loaded -> update block light colors
		level->update_light_colors();
	}
	catch(a2e_exception& exc) {
		a2e_error("failed to read map %s: %s!", filename, exc.what());
		delete level;
		return nullptr;
	}
	catch(...) {
		a2e_error("failed to read map %s!", filename);
		delete level;
		return nullptr;
	}
	
	// done
	a2e_debug("loaded map %s", filename);
	active_map = level;
	ge->set_status(GAME_STATUS::IDLE);
	eevt->add_event(EVENT_TYPE::MAP_LOAD, make_shared<map_load_event>(SDL_GetTicks(), filename));
	return level;
}

bool map_storage::load_map_data(file_io& file, sb_map& level) {
	uint3 chunk_count;
	chunk_count.x = file.get_uint();
	chunk_count.y = file.get_uint();
	chunk_count.z = file.get_uint();
	const size_t total_chunk_count = chunk_count.x * chunk_count.y * chunk_count.z;
	a2e_debug(":: chunk count: %v (total: %u, blocks: %u)", chunk_count, total_chunk_count,
			  total_chunk_count * sb_map::blocks_per_chunk);
	
	uint3 player_start;
	player_start.x = file.get_uint();
	player_start.y = file.get_uint();
	player_start.z = file.get_uint();
	level.set_player_start(player_start);
	
	float3 player_rotation;
	player_rotation.x = file.get_float();
	player_rotation.y = file.get_float();
	player_rotation.z = file.get_float();
	level.set_player_rotation(player_rotation);
	
	float3 default_light_color;
	default_light_color.x = file.get_float();
	default_light_color.y = file.get_float();
	default_light_color.z = file.get_float();
	level.set_default_light_color(default_light_color);
	
	if(file.fail()) throw a2e_exception("read/extraction fail (in map header)");
	if(chunk_count.x == 0 ||
	   chunk_count.y == 0 ||
	   chunk_count.z == 0) {
		throw a2e_exception("chunk count can't be 0");
	}
	
	// init map data
	level.resize(chunk_count);
	for(unsigned int chunk_counter = 0; chunk_counter < total_chunk_count; chunk_counter++) {
		for(unsigned int block_counter = 0; block_counter < sb_map::blocks_per_chunk; block_counter++) {
			level.update(chunk_counter, sb_map::block_index_to_position(block_counter), (BLOCK_MATERIAL)file.get_uint());
		}
		if(file.fail()) throw a2e_exception("read/extraction fail (in chunk #"+uint2string(chunk_counter)+")");
	}
	
	return true;
}

bool map_storage::load_audio_background(file_io& file, sb_map& level) {
	string filename = "";
	file.get_terminated_block(filename, 0);
	string identifier = "";
	file.get_terminated_block(identifier, 0);
	// ignore old identifier -> new identifier: uppercase filename (-bg_, -.mp3)
	identifier = core::str_to_upper(filename.substr(3, filename.size() - 7));
	
	const float volume = file.get_float();
	
	file.get_uint(); // unnecessary play_on_load
	
	ac->acquire_context();
	if(as->load_file(e->data_path("music/"+filename), identifier) == nullptr) {
		ac->release_context();
		throw a2e_exception("failed to load background audio: "+filename+" ("+identifier+")");
	}
	ac->release_context();
	
	audio_background* au = ac->add_audio_background(identifier, "0");
	if(au == nullptr) {
		throw a2e_exception("failed to create background audio for: "+identifier+".0");
	}
	
	au->set_volume(volume);
	au->play(); // always play on load
	
	level.set_background_music(au);
	
	return true;
}

bool map_storage::load_audio_3d(file_io& file, sb_map& level) {
	string filename = "";
	file.get_terminated_block(filename, 0);
	string identifier = "";
	file.get_terminated_block(identifier, 0);
	
	uint3 position;
	position.x = file.get_uint();
	position.y = file.get_uint();
	position.z = file.get_uint();
	
	const bool play_on_load = (file.get_uint() == 0 ? false : true);
	const bool loop = (file.get_uint() == 0 ? false : true);
	const bool override_defaults = (file.get_uint() == 0 ? false : true);
	float3 velocity;
	float volume = 0.0f;
	float ref_dist = 0.0f;
	float rolloff_factor = 0.0f;
	float max_dist = 0.0f;
	if(override_defaults) {
		velocity.x = file.get_float();
		velocity.y = file.get_float();
		velocity.z = file.get_float();
		
		volume = file.get_float();
		ref_dist = file.get_float();
		rolloff_factor = file.get_float();
		max_dist = file.get_float();
	}
	
	ac->acquire_context();
	if(as->load_file(e->data_path("music/"+filename), identifier) == nullptr) {
		ac->release_context();
		throw a2e_exception("failed to load 3d audio: "+filename+" ("+identifier+")");
	}
	ac->release_context();
	
	audio_3d* au = ac->add_audio_3d(identifier, "0");
	if(au == nullptr) {
		throw a2e_exception("failed to create 3d audio for: "+identifier+".0");
	}
	au->set_position(float3(position) + 0.5f);
	
	if(override_defaults) {
		au->set_velocity(velocity);
		au->set_volume(volume);
		au->set_reference_distance(ref_dist);
		au->set_rolloff_factor(rolloff_factor);
		au->set_max_distance(max_dist);
	}
	
	if(loop) au->loop();
	if(play_on_load) {
		au->set_play_on_load(true);
		au->play();
	}
	
	level.add_sound(au);
	
	return true;
}

bool map_storage::load_map_link(file_io& file, sb_map& level) {
	string dst_map_name = "";
	file.get_terminated_block(dst_map_name, 0);
	if(dst_map_name.length() == 0) {
		throw a2e_exception("no destination map specified for map link");
	}
	
	string identifier = "";
	file.get_terminated_block(identifier, 0);
	if(identifier.length() == 0) {
		throw a2e_exception("no identifier specified for map link");
	}
	
	const bool enabled = (file.get_uint() == 0 ? false : true);
	
	const unsigned int pos_count = file.get_uint();
	vector<uint3> positions;
	for(unsigned int i = 0; i < pos_count; i++) {
		uint3 position;
		position.x = file.get_uint();
		position.y = file.get_uint();
		position.z = file.get_uint();
		positions.emplace_back(position);
	}
	
	sb_map::map_link* ml = new sb_map::map_link {
		dst_map_name,
		identifier,
		positions,
		enabled
	};
	
	level.add_map_link(ml);
	return true;
}

bool map_storage::load_trigger(file_io& file, sb_map& level) {
	string identifier = "";
	file.get_terminated_block(identifier, 0);
	if(identifier.length() == 0) {
		throw a2e_exception("no identifier specified for trigger");
	}
	
	const TRIGGER_TYPE type((TRIGGER_TYPE)file.get_uint());
	const TRIGGER_SUB_TYPE sub_type((TRIGGER_SUB_TYPE)file.get_uint());
	
	uint3 position;
	position.x = file.get_uint();
	position.y = file.get_uint();
	position.z = file.get_uint();
	
	const BLOCK_FACE facing = (BLOCK_FACE)file.get_uint();
	if((unsigned int)facing == 0 || facing > BLOCK_FACE::ALL) {
		throw a2e_exception("invalid trigger facing: "+uint2string((unsigned int)facing));
	}
	
	string script_filename = "";
	file.get_terminated_block(script_filename, 0);
	if(script_filename.length() == 0) {
		throw a2e_exception("no script filename specified for trigger");
	}
	
	// note: these are allowed to be empty!
	string on_load = "", on_trigger = "", on_untrigger = "";
	file.get_terminated_block(on_load, 0);
	file.get_terminated_block(on_trigger, 0);
	file.get_terminated_block(on_untrigger, 0);
	
	float weight = 0.0f, intensity = 0.0f;
	unsigned int time = 0;
	switch(type) {
		case TRIGGER_TYPE::WEIGHT:
			weight = file.get_float();
			break;
		case TRIGGER_TYPE::LIGHT:
			intensity = file.get_float();
			break;
		default: break;
	}
	
	switch(sub_type) {
		case TRIGGER_SUB_TYPE::NONE: break;
		case TRIGGER_SUB_TYPE::TIMED:
			time = file.get_uint();
			break;
	}
	
	level.add_trigger(new sb_map::trigger {
		identifier,
		type,
		sub_type,
		position,
		facing,
		sh->load_script(script_filename),
		on_load,
		on_trigger,
		on_untrigger,
		// dependent data:
		weight,
		intensity,
		time,
		sb_map::trigger::state_struct()
	});
	
	return true;
}

bool map_storage::load_light_color_area(file_io& file, sb_map& level) {
	uint3 min_pos, max_pos;
	min_pos.x = file.get_uint();
	min_pos.y = file.get_uint();
	min_pos.z = file.get_uint();
	max_pos.x = file.get_uint();
	max_pos.y = file.get_uint();
	max_pos.z = file.get_uint();
	
	float3 color;
	color.x = file.get_float();
	color.y = file.get_float();
	color.z = file.get_float();
	
	level.add_light_color_area(new sb_map::light_color_area {
		min_pos,
		max_pos,
		color
	});
	
	return true;
}

bool map_storage::load_ai_waypoint(file_io& file, sb_map& level) {
	string identifier = "";
	file.get_terminated_block(identifier, 0);
	if(identifier.length() == 0) {
		throw a2e_exception("no identifier specified for ai waypoint");
	}
	
	string next_waypoint = "";
	file.get_terminated_block(next_waypoint, 0);
	
	uint3 position;
	position.x = file.get_uint();
	position.y = file.get_uint();
	position.z = file.get_uint();
	
	sb_map::ai_waypoint* wp = new sb_map::ai_waypoint {
		identifier,
		nullptr,
		position
	};
	level.add_ai_waypoint(wp);
	ai_waypoint_resolve_map.insert({wp, next_waypoint});
	
	return true;
}

bool map_storage::save(const string& filename, const sb_map& level) {
	file_io file(e->data_path("maps/"+filename), file_io::OPEN_TYPE::WRITE_BINARY);
	if(!file.is_open()) {
		a2e_error("couldn't save level %s!", e->data_path("maps/"+filename));
		return false;
	}
	
	try {
		// header
		file.write_uint(header_magic);
		file.write_uint(sb_map::map_version);
		file.write_terminated_block(level.get_name(), 0);
		const size_t struct_count_pos = file.get_current_offset(); // save current position, since we don't know the struct count yet
		file.write_uint(uint_placeholder);
		unsigned int struct_count = 0;
		
		//
		const auto struct_writer_func = [&file, &level, &struct_count](DATA_TYPES type, std::function<void()> save_func) -> void {
			file.write_uint((unsigned int)type); // type char[4]
			file.write_uint(data_versions.at((unsigned int)type)); // version
			const size_t struct_length_pos = file.get_current_offset();
			file.write_uint(uint_placeholder); // struct length placeholder
			
			save_func();
			
			const size_t end_pos = file.get_current_offset();
			file.seek(struct_length_pos);
			file.write_uint((unsigned int)(end_pos - struct_length_pos - sizeof(unsigned int))); // write actual length
			file.seek(end_pos); // seek back to the end
			
			struct_count++;
		};
		
		// structs
		save_map_data(file, level, struct_writer_func);
		save_audio_background(file, level, struct_writer_func);
		save_audio_3d(file, level, struct_writer_func);
		save_map_link(file, level, struct_writer_func);
		save_trigger(file, level, struct_writer_func);
		save_light_color_area(file, level, struct_writer_func);
		save_ai_waypoint(file, level, struct_writer_func);
		
		// write struct count
		file.seek(struct_count_pos);
		file.write_uint(struct_count);
	}
	catch(a2e_exception& exc) {
		a2e_error("failed to write map %s: %s!", filename, exc.what());
		return false;
	}
	catch(...) {
		a2e_error("failed to write map %s!", filename);
		return false;
	}
	
	file.close();
	
	return true;
}

bool map_storage::save_map_data(file_io& file, const sb_map& level, struct_writer writer) {
	writer(DATA_TYPES::MAP_DATA, [&file, &level]() {
		const uint3 chunk_count(level.get_chunk_count());
		file.write_uint(chunk_count.x);
		file.write_uint(chunk_count.y);
		file.write_uint(chunk_count.z);
		
		file.write_uint(level.get_player_start().x);
		file.write_uint(level.get_player_start().y);
		file.write_uint(level.get_player_start().z);
		
		file.write_float(level.get_player_rotation().x);
		file.write_float(level.get_player_rotation().y);
		file.write_float(level.get_player_rotation().z);
		
		file.write_float(level.get_default_light_color().x);
		file.write_float(level.get_default_light_color().y);
		file.write_float(level.get_default_light_color().z);
		
		const unsigned int total_chunk_count = chunk_count.x * chunk_count.y * chunk_count.z;
		for(unsigned int chunk_index = 0; chunk_index < total_chunk_count; chunk_index++) {
			for(unsigned int block_index = 0; block_index < sb_map::blocks_per_chunk; block_index++) {
				file.write_uint((unsigned int)level.get_block(chunk_index, block_index).material);
			}
		}
	});
	return true;
}

bool map_storage::save_audio_background(file_io& file, const sb_map& level, struct_writer writer) {
	const auto& music = level.get_background_music();
	if(music != nullptr) {
		writer(DATA_TYPES::AUDIO_BACKGROUND, [&file, &level, &music]() {
			//
			const string& combined_identifier(music->get_identifier());
			const size_t point_pos = combined_identifier.find(".");
			if(point_pos == string::npos) {
				throw a2e_exception("invalid background audio identifier: "+combined_identifier);
			}
			const string identifier(combined_identifier.substr(0, point_pos));
			const audio_store::audio_data& data(as->get_audio_data(identifier));
			
			const size_t slash_pos = data.filename.rfind("/");
			if(slash_pos == string::npos) {
				throw a2e_exception("invalid background audio filename: "+data.filename);
			}
			
			file.write_terminated_block(data.filename.substr(slash_pos+1, data.filename.size()-slash_pos-1), 0);
			file.write_terminated_block(identifier, 0);
			file.write_float(music->get_volume());
			file.write_uint(music->is_playing() ? 1 : 0);
		});
	}
	return true;
}

bool map_storage::save_audio_3d(file_io& file, const sb_map& level, struct_writer writer) {
	for(const auto& sound : level.get_sounds()) {
		writer(DATA_TYPES::AUDIO_3D, [&file, &level, &sound]() {
			//
			const string& combined_identifier(sound->get_identifier());
			const size_t point_pos = combined_identifier.find(".");
			if(point_pos == string::npos) {
				throw a2e_exception("invalid 3d audio identifier: "+combined_identifier);
			}
			const string identifier(combined_identifier.substr(0, point_pos));
			const audio_store::audio_data& data(as->get_audio_data(identifier));
			
			const size_t slash_pos = data.filename.rfind("/");
			if(slash_pos == string::npos) {
				throw a2e_exception("invalid 3d audio filename: "+data.filename);
			}
			
			file.write_terminated_block(data.filename.substr(slash_pos+1, data.filename.size()-slash_pos-1), 0);
			file.write_terminated_block(identifier, 0);
			
			file.write_uint((unsigned int)floorf(sound->get_position().x));
			file.write_uint((unsigned int)floorf(sound->get_position().y));
			file.write_uint((unsigned int)floorf(sound->get_position().z));
			
			file.write_uint(sound->get_play_on_load() ? 1 : 0);
			file.write_uint(sound->is_looping() ? 1 : 0);
			
			const float3& velocity(sound->get_velocity());
			const float& volume(sound->get_volume());
			const float& ref_dist(sound->get_reference_distance());
			const float& rolloff_fac(sound->get_rolloff_factor());
			const float& max_dist(sound->get_max_distance());
			
			if((velocity != audio_store::default_velocity).any() ||
			   volume != audio_store::default_volume ||
			   ref_dist != audio_store::default_reference_distance ||
			   rolloff_fac != audio_store::default_rolloff_factor ||
			   max_dist != audio_store::default_max_distance) {
				file.write_uint(1); // doesn't use defaults (-> override)
				file.write_float(velocity.x);
				file.write_float(velocity.y);
				file.write_float(velocity.z);
				file.write_float(volume);
				file.write_float(ref_dist);
				file.write_float(rolloff_fac);
				file.write_float(max_dist);
			}
			else file.write_uint(0); // does use defaults
		});
	}
	return true;
}

bool map_storage::save_map_link(file_io& file, const sb_map& level, struct_writer writer) {
	for(const auto& ml : level.get_map_links()) {
		writer(DATA_TYPES::MAP_LINK, [&file, &level, &ml]() {
			file.write_terminated_block(ml->dst_map_name, 0);
			file.write_terminated_block(ml->identifier, 0);
			file.write_uint(ml->enabled ? 1 : 0);
			file.write_uint((unsigned int)ml->positions.size());
			for(const auto& pos : ml->positions) {
				file.write_uint(pos.x);
				file.write_uint(pos.y);
				file.write_uint(pos.z);
			}
		});
	}
	return true;
}

bool map_storage::save_trigger(file_io& file, const sb_map& level, struct_writer writer) {
	for(const auto& trgr : level.get_triggers()) {
		writer(DATA_TYPES::TRIGGER, [&file, &level, &trgr]() {
			file.write_terminated_block(trgr->identifier, 0);
			file.write_uint((unsigned int)trgr->type);
			file.write_uint((unsigned int)trgr->sub_type);
			
			file.write_uint(trgr->position.x);
			file.write_uint(trgr->position.y);
			file.write_uint(trgr->position.z);
			
			file.write_uint((unsigned int)trgr->facing);
			
			const string filename(trgr->scr->get_filename());
			const size_t slash_pos = filename.rfind("/");
			if(slash_pos == string::npos) {
				throw a2e_exception("invalid script filename: "+filename);
			}
			file.write_terminated_block(filename.substr(slash_pos+1, filename.size()-slash_pos-1), 0);
			
			file.write_terminated_block(trgr->on_load, 0);
			file.write_terminated_block(trgr->on_trigger, 0);
			file.write_terminated_block(trgr->on_untrigger, 0);
			
			switch(trgr->type) {
				case TRIGGER_TYPE::WEIGHT:
					file.write_float(trgr->weight);
					break;
				case TRIGGER_TYPE::LIGHT:
					file.write_float(trgr->intensity);
					break;
				default: break;
			}
			
			switch(trgr->sub_type) {
				case TRIGGER_SUB_TYPE::NONE: break;
				case TRIGGER_SUB_TYPE::TIMED:
					file.write_uint(trgr->time);
					break;
			}
		});
	}
	return true;
}

bool map_storage::save_light_color_area(file_io& file, const sb_map& level, struct_writer writer) {
	for(const auto& lca : level.get_light_color_areas()) {
		writer(DATA_TYPES::LIGHT_COLOR_AREA, [&file, &level, &lca]() {
			file.write_uint(lca->min_pos.x);
			file.write_uint(lca->min_pos.y);
			file.write_uint(lca->min_pos.z);
			
			file.write_uint(lca->max_pos.x);
			file.write_uint(lca->max_pos.y);
			file.write_uint(lca->max_pos.z);
			
			file.write_float(lca->color.x);
			file.write_float(lca->color.y);
			file.write_float(lca->color.z);
		});
	}
	return true;
}

bool map_storage::save_ai_waypoint(file_io& file, const sb_map& level, struct_writer writer) {
	for(const auto& wp : level.get_ai_waypoints()) {
		writer(DATA_TYPES::AI_WAYPOINT, [&file, &level, &wp]() {
			file.write_terminated_block(wp->identifier, 0);
			file.write_terminated_block(wp->next == nullptr ? "" : wp->next->identifier, 0);
			
			file.write_uint(wp->position.x);
			file.write_uint(wp->position.y);
			file.write_uint(wp->position.z);
		});
	}
	return true;
}
