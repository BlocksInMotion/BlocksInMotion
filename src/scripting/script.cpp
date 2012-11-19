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

#include "script.h"
#include "script_handler.h"
#include "sb_map.h"
#include "audio_store.h"
#include "audio_controller.h"
#include "audio_3d.h"
#include <scene/light.h>

script::script(const string& filename_) : filename(filename_) {
	load(filename);
}

script::~script() {
}

void script::reload() {
	functions.clear();
	load(filename);
}

const string& script::get_filename() const {
	return filename;
}

void script::load(const string& filename_) {
	stringstream buffer(ios::in | ios::out);
	if(!file_io::file_to_buffer(filename_, buffer)) {
		a2e_error("failed to read script \"%s\"!", filename_);
		return;
	}
	
	string line_str = "", token = "", cur_identifier = "";
	script_function* cur_script = nullptr;
	bool in_block = false;
	while(getline(buffer, line_str)) {
		istringstream line(line_str);
		while(!line.eof()) {
			line >> token;
			if(line.fail() || line.bad()) break;
			if(token.size() >= 2 && token.substr(0, 2) == "//") {
				// ignore comment / rest of the line
				break;
			}
			else if(token == "{") {
				in_block = true;
				continue;
			}
			else if(token == "}") {
				in_block = false;
				continue;
			}
			
			if(!in_block) {
				// outside of a block, this must be a new identifier
				cur_identifier = token;
				functions.insert(make_pair(cur_identifier, script_function()));
				cur_script = &functions[cur_identifier];
			}
			else {
				if(cur_script == nullptr) break;
				// add line to current script and continue with next line
				cur_script->lines.emplace_back(core::tokenize(token +
															  (line.tellg() > 0 ?
															   line_str.substr(line.tellg(), line_str.size() - line.tellg()) :
															   ""),
															  ' '));
				break;
			}
		}
	}
}

void script::execute(sb_map* cur_map, const string& identifier, const uint3 position) const {
	if(functions.count(identifier) == 0) {
		a2e_error("function \"%s\" not found in script \"%s\"!", identifier, filename);
		return;
	}
	
	if(cur_map == nullptr) {
		a2e_error("can't execute function \"%s\" in script \"%s\", because there is no active map!", identifier, filename);
		return;
	}
	
	// helper functions (string <-> type conversion)
	static const unordered_map<string, COMMAND> commands {
		{ "nop", COMMAND::NOP },
		{ "add", COMMAND::ADD_BLOCK },
		{ "delete", COMMAND::DELETE_BLOCK },
		{ "modify", COMMAND::MODIFY_BLOCK },
		{ "toggle", COMMAND::TOGGLE_BLOCK },
		{ "make_dynamic", COMMAND::MAKE_DYNAMIC },
		{ "call", COMMAND::CALL },
		{ "call_script", COMMAND::CALL_SCRIPT },
		{ "activate", COMMAND::ACTIVATE },
		{ "deactivate", COMMAND::DEACTIVATE },
		{ "trigger", COMMAND::TRIGGER },
		{ "play_sound", COMMAND::PLAY_SOUND },
		{ "kill_lights", COMMAND::KILL_LIGHTS },
		{ "light_color", COMMAND::LIGHT_COLOR },
		{ "delete_trigger", COMMAND::DELETE_TRIGGER },
		{ "move_trigger", COMMAND::MOVE_TRIGGER },
	};
	const auto valid_cmd_token_count = [](const COMMAND& cmd, const vector<string>& tokens) -> bool {
		static const unordered_map<unsigned int, size_t> command_token_count {
			{ (unsigned int)COMMAND::ADD_BLOCK, 5 },
			{ (unsigned int)COMMAND::DELETE_BLOCK, 4 },
			{ (unsigned int)COMMAND::MODIFY_BLOCK, 5 },
			{ (unsigned int)COMMAND::TOGGLE_BLOCK, 6 },
			{ (unsigned int)COMMAND::MAKE_DYNAMIC, 4 },
			{ (unsigned int)COMMAND::CALL, 2 },
			{ (unsigned int)COMMAND::CALL_SCRIPT, 3 },
			{ (unsigned int)COMMAND::ACTIVATE, 3 },
			{ (unsigned int)COMMAND::DEACTIVATE, 3 },
			{ (unsigned int)COMMAND::TRIGGER, 4 },
			{ (unsigned int)COMMAND::PLAY_SOUND, 5 },
			{ (unsigned int)COMMAND::KILL_LIGHTS, 7 },
			{ (unsigned int)COMMAND::LIGHT_COLOR, 7 },
			{ (unsigned int)COMMAND::DELETE_TRIGGER, 2 },
			{ (unsigned int)COMMAND::MOVE_TRIGGER, 5 },
		};
		return (command_token_count.count((unsigned int)cmd) == 0 ?
				false : command_token_count.at((unsigned int)cmd) == tokens.size());
	};
	const auto token_to_cmd = [&identifier,this](const string token) -> COMMAND {
		if(commands.count(token) == 0) {
			a2e_error("invalid command \"%s\" in function \"%s\" in script \"%s\"!", token, identifier, filename);
			return COMMAND::NOP;
		}
		return commands.at(token);
	};
	const auto token_to_mat = [&identifier,this](const string& token) -> BLOCK_MATERIAL {
		static const unordered_map<string, BLOCK_MATERIAL> materials {
			{ "NONE", BLOCK_MATERIAL::NONE },
			{ "INDESTRUCTIBLE", BLOCK_MATERIAL::INDESTRUCTIBLE },
			{ "METAL", BLOCK_MATERIAL::METAL },
			{ "MAGNET", BLOCK_MATERIAL::MAGNET },
			{ "LIGHT", BLOCK_MATERIAL::LIGHT },
			{ "ACID", BLOCK_MATERIAL::ACID },
			{ "SPRING", BLOCK_MATERIAL::SPRING },
			{ "SPAWNER", BLOCK_MATERIAL::SPAWNER },
		};
		if(materials.count(token) == 0) {
			a2e_error("invalid material \"%s\" in function \"%s\" in script \"%s\"!", token, identifier, filename);
			return BLOCK_MATERIAL::NONE;
		}
		return materials.at(token);
	};
	const auto pos_tokens_to_pos = [&position](const string& tok_x, const string& tok_y, const string& tok_z) -> uint3 {
		uint3 pos;
		if(tok_x[0] == '#') pos.x = position.x + string2uint(tok_x.substr(1, tok_x.length() - 1));
		else pos.x = string2uint(tok_x);
		if(tok_y[0] == '#') pos.y = position.y + string2uint(tok_y.substr(1, tok_y.length() - 1));
		else pos.y = string2uint(tok_y);
		if(tok_z[0] == '#') pos.z = position.z + string2uint(tok_z.substr(1, tok_z.length() - 1));
		else pos.z = string2uint(tok_z);
		return pos;
	};
	
	// handle commands
	COMMAND cur_cmd = COMMAND::NOP;
	string cur_cmd_str = "INVALID";
	const auto script_error = [&cur_cmd,&cur_cmd_str,&identifier,this](const string msg) {
		a2e_error("failed to execute command \"%s\" in function \"%s\" in script \"%s\": %s!",
				  cur_cmd_str, identifier, filename, msg);
	};
	const script_function& func(functions.at(identifier));
	for(const auto& cmd : func.lines) {
		cur_cmd = token_to_cmd(cmd[0]);
		cur_cmd_str = cmd[0];
		if(cur_cmd != COMMAND::NOP &&
		   !valid_cmd_token_count(cur_cmd, cmd)) {
			script_error("invalid token count "+size_t2string(cmd.size())+" for command \""+cmd[0]+"\"");
			continue;
		}
		switch(cur_cmd) {
			case COMMAND::ADD_BLOCK: {
				const BLOCK_MATERIAL mat(token_to_mat(cmd[1]));
				const uint3 pos(pos_tokens_to_pos(cmd[2], cmd[3], cmd[4]));
				if(!cur_map->is_valid_position(pos)) {
					script_error("invalid position "+pos.to_string());
					break;
				}
				const sb_map::block_data& data(cur_map->get_block(pos));
				if(data.material != BLOCK_MATERIAL::NONE) {
					script_error("can't add block: there is already a block at position "+pos.to_string()+
								 " of material "+uint2string((unsigned int)data.material));
					break;
				}
				cur_map->update(pos, mat);
			}
			break;
			case COMMAND::DELETE_BLOCK: {
				const uint3 pos(pos_tokens_to_pos(cmd[1], cmd[2], cmd[3]));
				if(!cur_map->is_valid_position(pos)) {
					script_error("invalid position "+pos.to_string());
					break;
				}
				const sb_map::block_data& data(cur_map->get_block(pos));
				if(data.material == BLOCK_MATERIAL::NONE) {
					script_error("can't delete block: there is no block at position "+pos.to_string());
					break;
				}
				cur_map->update(pos, BLOCK_MATERIAL::NONE);
			}
			break;
			case COMMAND::MODIFY_BLOCK: {
				const BLOCK_MATERIAL mat(token_to_mat(cmd[1]));
				const uint3 pos(pos_tokens_to_pos(cmd[2], cmd[3], cmd[4]));
				if(!cur_map->is_valid_position(pos)) {
					script_error("invalid position "+pos.to_string());
					break;
				}
				cur_map->update(pos, mat);
			}
			break;
			case COMMAND::TOGGLE_BLOCK: {
				const array<BLOCK_MATERIAL, 2> mats { { token_to_mat(cmd[1]), token_to_mat(cmd[2]) } };
				const uint3 pos(pos_tokens_to_pos(cmd[3], cmd[4], cmd[5]));
				if(!cur_map->is_valid_position(pos)) {
					script_error("invalid position "+pos.to_string());
					break;
				}
				const BLOCK_MATERIAL cur_mat(cur_map->get_block(pos).material);
				if(cur_mat != mats[0] && cur_mat != mats[1]) {
					script_error("toggle destination material ("+uint2string((unsigned int)cur_mat)+
								 ") fits neither valid material ("+cmd[1]+", "+cmd[2]+")");
				}
				else {
					cur_map->update(pos, mats[(cur_mat == mats[0]) ? 1 : 0]);
				}
			}
			break;
			case COMMAND::MAKE_DYNAMIC: {
				const uint3 pos(pos_tokens_to_pos(cmd[1], cmd[2], cmd[3]));
				if(!cur_map->is_valid_position(pos)) {
					script_error("invalid position "+pos.to_string());
					break;
				}
				cur_map->make_dynamic(cur_map->chunk_position_to_index(pos / sb_map::chunk_extent),
									  sb_map::block_position_to_index(pos % sb_map::chunk_extent));
			}
			break;
			case COMMAND::CALL:
				if(functions.count(cmd[1]) == 0) {
					script_error("function \""+cmd[1]+"\" does not exist");
					break;
				}
				execute(cur_map, cmd[1], position);
				break;
			case COMMAND::CALL_SCRIPT: {
				script* scr = sh->load_script(cmd[1]);
				if(scr == nullptr) {
					script_error("couldn't execute script \""+cmd[1]+"\"");
					break;
				}
				scr->execute(cur_map, cmd[2], position);
			}
			break;
			case COMMAND::DEACTIVATE:
			case COMMAND::ACTIVATE: {
				const bool state(cur_cmd == COMMAND::ACTIVATE);
				if(cmd[1] == "door") {
					sb_map::map_link* ml = cur_map->get_map_link(cmd[2]);
					if(ml == nullptr) {
						script_error("there is no map link with the name \""+cmd[2]+"\"");
						break;
					}
					ml->enabled = state;
				}
				else if(cmd[1] == "trigger") {
					sb_map::trigger* trgr = cur_map->get_trigger(cmd[2]);
					if(trgr == nullptr) {
						script_error("there is no trigger with the name \""+cmd[2]+"\"");
						break;
					}
					if(cur_cmd == COMMAND::ACTIVATE) trgr->activate(cur_map);
					else trgr->deactivate(cur_map);
				}
				else if(cmd[2] == "sound") {
					audio_3d* snd = cur_map->get_sound(cmd[2]);
					if(snd == nullptr) {
						script_error("there is no sound with the name \""+cmd[2]+"\"");
						break;
					}
					if(cur_cmd == COMMAND::ACTIVATE) snd->play();
					else snd->stop();
				}
			}
			break;
			case COMMAND::TRIGGER: {
				const uint3 pos(pos_tokens_to_pos(cmd[1], cmd[2], cmd[3]));
				if(!cur_map->is_valid_position(pos)) {
					script_error("invalid position "+pos.to_string());
					break;
				}
				
				// note: this will fail silently when there is no trigger at that position
				// -> that way, more generic functions can be written, w/o taking care of any special cases (e.g. end of a loop)
				const auto& triggers(cur_map->get_triggers());
				for(const auto& trgr : triggers) {
					if((trgr->position == pos).all()) {
						trgr->activate(cur_map);
						break;
					}
				}
			}
			break;
			case COMMAND::PLAY_SOUND: {
				if(!as->has_audio_data(cmd[1])) {
					script_error("sound \""+cmd[1]+"\" does not exist");
					break;
				}
				const uint3 pos(pos_tokens_to_pos(cmd[2], cmd[3], cmd[4]));
				if(!cur_map->is_valid_position(pos)) {
					script_error("invalid position "+pos.to_string());
					break;
				}
				audio_3d* src = ac->add_audio_3d(cmd[1], identifier+"."+size_t2string(SDL_GetPerformanceCounter()), true);
				src->set_position(float3(pos) + 0.5f);
				src->play();
			}
			break;
			case COMMAND::KILL_LIGHTS: {
				const uint3 pos_0(pos_tokens_to_pos(cmd[1], cmd[2], cmd[3]));
				if(!cur_map->is_valid_position(pos_0)) {
					script_error("invalid position "+pos_0.to_string());
					break;
				}
				const uint3 pos_1(pos_tokens_to_pos(cmd[4], cmd[5], cmd[6]));
				if(!cur_map->is_valid_position(pos_1)) {
					script_error("invalid position "+pos_1.to_string());
					break;
				}
				for(unsigned int px = pos_0.x; px <= pos_1.x; px++) {
					for(unsigned int py = pos_0.y; py <= pos_1.y; py++) {
						for(unsigned int pz = pos_0.z; pz <= pos_1.z; pz++) {
							const uint3 pos(px, py, pz);
							if(cur_map->get_block(pos).material == BLOCK_MATERIAL::LIGHT) {
								cur_map->update(pos, BLOCK_MATERIAL::NONE);
							}
						}
					}
				}
			}
			break;
			case COMMAND::LIGHT_COLOR: {
				const uint3 pos(pos_tokens_to_pos(cmd[1], cmd[2], cmd[3]));
				if(!cur_map->is_valid_position(pos)) {
					script_error("invalid position "+pos.to_string());
					break;
				}
				light* li = cur_map->get_light(pos);
				if(li == nullptr) {
					script_error("there is no light at position "+pos.to_string());
					break;
				}
				li->set_color(string2float(cmd[4]), string2float(cmd[5]), string2float(cmd[6]));
			}
			break;
			case COMMAND::DELETE_TRIGGER: {
				sb_map::trigger* trgr = cur_map->get_trigger(cmd[1]);
				if(trgr == nullptr) {
					script_error("there is no trigger with the name \""+cmd[1]+"\"");
					break;
				}
				cur_map->remove_trigger(trgr);
			}
			break;
			case COMMAND::MOVE_TRIGGER: {
				sb_map::trigger* trgr = cur_map->get_trigger(cmd[1]);
				if(trgr == nullptr) {
					script_error("there is no trigger with the name \""+cmd[1]+"\"");
					break;
				}
				const uint3 pos(pos_tokens_to_pos(cmd[2], cmd[3], cmd[4]));
				if(!cur_map->is_valid_position(pos)) {
					script_error("invalid position "+pos.to_string());
					break;
				}
				trgr->position = pos;
			}
			break;
			case COMMAND::NOP:
				break;
		}
	}
}

const unordered_map<string, script::script_function>& script::get_functions() const {
	return functions;
}
