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

#include <BulletSoftBody/btSoftBody.h>
#include "sb_console.h"
#include <engine.h>
#include <rendering/gfx2d.h>
#include <gui/font.h>
#include <gui/font_manager.h>
#include <a2e_version.h>
#include <scene/scene.h>
#include "physics_controller.h"
#include "rigid_body.h"
#include "soft_body.h"
#include "sb_map.h"
#include "map_storage.h"
#include "editor.h"
#include "script.h"
#include "script_handler.h"
#include "game.h"
#include "save.h"
#include <scene/camera.h>

constexpr size_t sb_console::display_line_count;
constexpr float sb_console::margin;

const unordered_map<string, sb_console::COMMAND> sb_console::commands {
	{ "set", sb_console::COMMAND::SET },
	{ "get", sb_console::COMMAND::GET },
	{ "list", sb_console::COMMAND::LIST },
	{ "help", sb_console::COMMAND::HELP },
	{ "stats", sb_console::COMMAND::STATS },
	{ "load", sb_console::COMMAND::LOAD },
	{ "save", sb_console::COMMAND::SAVE },
	{ "editor", sb_console::COMMAND::EDITOR },
	{ "ed", sb_console::COMMAND::EDITOR },
	{ "run", sb_console::COMMAND::RUN },
};

static bool blink_state = false;
static void console_blink_handler(gui_simple_callback* ui_cb_obj) {
	static constexpr size_t blink_interval = 500;
	static size_t blink_timer = SDL_GetTicks();
	if(ui_cb_obj == nullptr) return;
	
	const size_t cur_ticks = SDL_GetTicks();
	if((cur_ticks - blink_timer) > blink_interval) {
		blink_timer = cur_ticks;
		blink_state ^= true;
		ui_cb_obj->redraw();
	}
}

sb_console::sb_console() :
draw_cb(this, &sb_console::draw_ui),
key_handler_fnctr(this, &sb_console::key_handler)
{
	fnt = fm->get_font("SYSTEM_MONOSPACE");
	
	add_line(u8"<b>"+e->get_version()+u8"</b>", false);
	add_line(u8"<b>"+APPLICATION_VERSION_STRING+u8"</b>", false);
	
	eevt->add_internal_event_handler(key_handler_fnctr, EVENT_TYPE::KEY_UP, EVENT_TYPE::UNICODE_INPUT);
}

sb_console::~sb_console() {
	eevt->remove_event_handler(key_handler_fnctr);
	if(enabled) set_enabled(false);
}

void sb_console::draw_ui(const DRAW_MODE_UI draw_mode a2e_unused, rtt::fbo* buffer) {
	const uint2 size(buffer->width, buffer->height);
	
	static const float2 origin(margin, margin);
	static const size_t glyph_size = fnt->get_display_size();
	static const size_t line_height = floor(float(glyph_size) * 1.125f); // WONTFIX: get line height from font? get font size from font
	size_t line_counter = 0;
	
	// background
	gfx2d::draw_rectangle_fill(rect(0, 0, size.x,
									(unsigned int)(origin.y * 2.0f) +
									(unsigned int)(line_height * ((display_lines.size() > display_line_count ?
												   display_line_count : display_lines.size()) + 1))),
							   float4(0.0f, 0.0f, 0.0f, 0.5f));
	
	// lines
	for_each(display_lines.size() > display_line_count ? cend(display_lines) - display_line_count : cbegin(display_lines),
			 cend(display_lines),
			 [&](const decltype(display_lines)::value_type& elem) {
				 fnt->draw_cached(elem.second.first.x, elem.second.first.y,
								  origin + float2(0.5f, 0.5f + float(line_counter * line_height)),
								  float4(0.0f, 0.0f, 0.0f, 1.0f));
				 fnt->draw_cached(elem.second.first.x, elem.second.first.y,
								  origin + float2(0.0f, float(line_counter * line_height)),
								  float4(1.0f));
				 line_counter++;
	});
	
	// cursor
	{
		const string blink = (blink_state ? u8"_" : u8" ");
		const string input_line(u8"> " + unicode::unicode_to_utf8(user_input) + (selected_input_char == -1 ? blink : u8""));
		fnt->draw(input_line,
				  origin + float2(0.5f, 0.5f + float(line_counter * line_height)),
				  float4(0.0f, 0.0f, 0.0f, 1.0f));
		fnt->draw(input_line,
				  origin + float2(0.0f, float(line_counter * line_height)),
				  float4(1.0f));
		
		if(selected_input_char != -1) {
			const float blink_x_offset = (glyph_size-5) * (selected_input_char + 2);
			fnt->draw(blink,
					  origin + float2(0.5f + blink_x_offset, 1.5f + float(line_counter * line_height)),
					  float4(0.0f, 0.0f, 0.0f, 1.0f));
			fnt->draw(blink,
					  origin + float2(0.0f + blink_x_offset, 1.0f + float(line_counter * line_height)),
					  float4(1.0f));
		}
	}
	
	// cleanup
	if(display_lines.size() > display_line_count) {
		const size_t drop_line_count = display_lines.size() - display_line_count;
		for(size_t i = 0; i < drop_line_count; i++) {
			glDeleteBuffers(1, &display_lines[0].second.first.x);
			display_lines.pop_front();
		}
	}
}

bool sb_console::key_handler(EVENT_TYPE type, shared_ptr<event_object> obj) {
	if(!enabled) return false;
	
	if(type == EVENT_TYPE::UNICODE_INPUT) {
		const shared_ptr<unicode_input_event>& key_evt = (shared_ptr<unicode_input_event>&)obj;
		
		// ignore tab
		if(key_evt->key == 0x09) {
			return true;
		}
		
		if(selected_input_char != -1) {
			if(selected_input_char >= 0 && selected_input_char < (ssize_t)user_input.size()) {
				user_input.emplace(begin(user_input) + selected_input_char, key_evt->key);
			}
			else a2e_error("line #%u: invalid selected_input_char %u (user_input: %u)", __LINE__, selected_input_char, user_input.size());
			selected_input_char++;
		}
		else {
			user_input.emplace_back(key_evt->key);
		}
		selected_input_line = -1;
		ui_cb_obj->redraw();
	}
	else if(type == EVENT_TYPE::KEY_UP) {
		const shared_ptr<key_up_event>& key_evt = (shared_ptr<key_up_event>&)obj;
		const SDL_Keymod mod = SDL_GetModState();
		bool redraw = true;
		switch(key_evt->key) {
			case SDLK_RETURN:
			case SDLK_RETURN2:
			case SDLK_KP_ENTER: {
				const string user_line_str(unicode::unicode_to_utf8(user_input));
				user_lines.emplace_back(user_line_str);
				add_line(user_line_str);
				user_input.clear();
				command_queue.push_back(user_line_str);
				selected_input_line = -1;
				selected_input_char = -1;
			}
			break;
			case SDLK_BACKSPACE:
				if(!user_input.empty()) {
					if(selected_input_char == -1) user_input.pop_back();
					else if(selected_input_char > 0) {
						selected_input_char--;
						if(selected_input_char >= 0 && selected_input_char < (ssize_t)user_input.size()) {
							user_input.erase(begin(user_input) + selected_input_char);
						}
						else a2e_error("line #%u: invalid selected_input_char %u (user_input: %u)", __LINE__, selected_input_char, user_input.size());
					}
					// else: nothing to do here
				}
				break;
			case SDLK_DELETE:
				if(!user_input.empty()) {
					if(selected_input_char != -1) {
						if(selected_input_char >= 0 && selected_input_char < (ssize_t)user_input.size()) {
							user_input.erase(begin(user_input) + selected_input_char);
						}
						else a2e_error("line #%u: invalid selected_input_char %u (user_input: %u)", __LINE__, selected_input_char, user_input.size());
						
						if(selected_input_char >= (ssize_t)user_input.size()) {
							selected_input_char = -1;
						}
					}
					// else: nothing to do here
				}
				break;
			case SDLK_UP:
				if(selected_input_line != 0 && !user_lines.empty()) {
					if(selected_input_line == -1) {
						selected_input_line = user_lines.size();
					}
					selected_input_line--;
					if(selected_input_line >= 0 && selected_input_line < (ssize_t)user_lines.size()) {
						user_input = unicode::utf8_to_unicode(user_lines[selected_input_line]);
					}
					else a2e_error("line #%u: invalid selected_input_line %u (user_lines: %u)", __LINE__, selected_input_line, user_lines.size());
					selected_input_char = -1;
				}
				break;
			case SDLK_DOWN:
				if(selected_input_line != -1) {
					selected_input_line++;
					if(selected_input_line < (ssize_t)user_lines.size()) {
						if(selected_input_line >= 0 && selected_input_line < (ssize_t)user_lines.size()) {
							user_input = unicode::utf8_to_unicode(user_lines[selected_input_line]);
						}
						else a2e_error("line #%u: invalid selected_input_line %u (user_lines: %u)", __LINE__, selected_input_line, user_lines.size());
					}
					else {
						user_input.clear();
						selected_input_line = -1;
					}
					selected_input_char = -1;
				}
				break;
			case SDLK_LEFT:
				if(selected_input_char != 0) {
					if(selected_input_char == -1) {
						selected_input_char = user_input.size();
					}
					selected_input_char--;
				}
				break;
			case SDLK_RIGHT:
				if(selected_input_char != -1) {
					selected_input_char++;
					if(selected_input_char >= (ssize_t)user_input.size()) {
						selected_input_char = -1;
					}
				}
				break;
			case SDLK_END:
				selected_input_char = -1;
				break;
			case SDLK_HOME:
				selected_input_char = 0;
				break;
			case SDLK_e:
				if((mod & (KMOD_RCTRL | KMOD_LCTRL)) != 0) {
					selected_input_char = -1;
				}
				break;
			case SDLK_a:
				if((mod & (KMOD_RCTRL | KMOD_LCTRL)) != 0) {
					selected_input_char = 0;
				}
				break;
			case SDLK_k:
				if((mod & (KMOD_RCTRL | KMOD_LCTRL)) != 0) {
					user_input.clear();
					selected_input_line = -1;
					selected_input_char = -1;
				}
				break;
			default:
				redraw = false;
				break;
		}
		if(redraw) ui_cb_obj->redraw();
	}
	return true;
}

void sb_console::add_line(const string& line_str, const bool add_input_character) {
	// WONTFIX: line break / figure out how many characters fit into one line
	display_lines.emplace_back(line_str, fnt->cache_text((add_input_character ? u8"> " : u8"  ") + line_str));
}

void sb_console::set_enabled(const bool state) {
	if(state == enabled) return;
	enabled = state;
	
	// also modify camera/game/editor control state
	if(ge != nullptr && ge->get_camera_control()) {
		ge->set_event_input(state ? false : true);
	}
	else if(ed == nullptr || !ed->is_enabled()) {
		cam->set_keyboard_input(state ? false : true);
		cam->set_wasd_input(state ? false : true);
	}
	
	if(enabled) {
		// note/WONTFIX: this won't resize correctly ...
		static const size_t line_height = floorf(float(fnt->get_display_size()) * 1.125f);
		static const unsigned int size_y = (unsigned int)(margin * 2.0f) + (unsigned int)(line_height * (display_line_count+1));
		ui_cb_obj = ui->add_draw_callback(DRAW_MODE_UI::POST_UI, draw_cb,
										  float2(1.0f, float(size_y)/float(e->get_height())), float2(0.0f),
										  gui_surface::SURFACE_FLAGS::NO_ANTI_ALIASING | gui_surface::SURFACE_FLAGS::NO_DEPTH);
	}
	else {
		ui_cb_obj = nullptr;
		ui->delete_draw_callback(draw_cb);
	}
}

bool sb_console::is_enabled() const {
	return enabled;
}

void sb_console::execute_cmd(const string& cmd_str) {
	if(cmd_str.empty()) return;
	const vector<string> tokens(core::tokenize(cmd_str, ' '));
	const auto iter = commands.find(tokens[0]);
	const COMMAND cmd = (iter != commands.end() ? iter->second : COMMAND::INVALID);
	
	switch(cmd) {
		case COMMAND::SET: {
			if(tokens.size() < 3) {
				add_line("not enough parameters: set <setting> <new value>", false);
				break;
			}
			const string& set_str(tokens[1]);
			const auto settings = conf::get_settings();
			const auto set_iter = settings.find(set_str);
			if(set_iter == settings.end()) {
				add_line("unknown config setting", false);
				break;
			}
			
			const string& new_val(tokens[2]);
			const size_t param_count = tokens.size() - 2;
			const auto vec_arg_split = [&](const size_t vec_size) -> vector<string> {
				vector<string> ret;
				const bool has_comma(new_val.find(",") != string::npos);
				if(!has_comma && param_count < vec_size) {
					add_line("not enough parameters: set <setting> <#0,#1,...> | set <setting> <0> <1> ...", false);
					return {};
				}
				if(has_comma) {
					ret = core::tokenize(new_val, ',');
					if(ret.size() < vec_size) {
						add_line("not enough parameters: set <setting> <#0,#1,...> | set <setting> <0> <1> ...", false);
						return {};
					}
				}
				else {
					for(size_t i = 0; i < vec_size; i++) {
						ret.emplace_back(tokens[2 + i]);
					}
				}
				return ret;
			};
			switch(set_iter->second.first) {
				case conf::CONF_TYPE::STRING: conf::set<string>(set_str, new_val); break;
				case conf::CONF_TYPE::BOOL: conf::set<bool>(set_str, string2bool(new_val)); break;
				case conf::CONF_TYPE::FLOAT: conf::set<float>(set_str, string2float(new_val)); break;
				case conf::CONF_TYPE::FLOAT2: {
					const vector<string> vec_vals(vec_arg_split(2));
					if(!vec_vals.empty()) conf::set<float2>(set_str, float2(string2float(vec_vals[0]),
																			string2float(vec_vals[1])));
				}
				break;
				case conf::CONF_TYPE::FLOAT3: {
					const vector<string> vec_vals(vec_arg_split(3));
					if(!vec_vals.empty()) conf::set<float3>(set_str, float3(string2float(vec_vals[0]),
																			string2float(vec_vals[1]),
																			string2float(vec_vals[2])));
				}
				break;
				case conf::CONF_TYPE::FLOAT4: {
					const vector<string> vec_vals(vec_arg_split(4));
					if(!vec_vals.empty()) conf::set<float4>(set_str, float4(string2float(vec_vals[0]),
																			string2float(vec_vals[1]),
																			string2float(vec_vals[2]),
																			string2float(vec_vals[3])));
				}
				break;
				case conf::CONF_TYPE::SIZE_T: conf::set<size_t>(set_str, string2size_t(new_val)); break;
				case conf::CONF_TYPE::SIZE2: {
					const vector<string> vec_vals(vec_arg_split(2));
					if(!vec_vals.empty()) conf::set<size2>(set_str, size2(string2size_t(vec_vals[0]),
																		  string2size_t(vec_vals[1])));
				}
				break;
				case conf::CONF_TYPE::SIZE3: {
					const vector<string> vec_vals(vec_arg_split(3));
					if(!vec_vals.empty()) conf::set<size3>(set_str, size3(string2size_t(vec_vals[0]),
																		  string2size_t(vec_vals[1]),
																		  string2size_t(vec_vals[2])));
				}
				break;
				case conf::CONF_TYPE::SIZE4: {
					const vector<string> vec_vals(vec_arg_split(4));
					if(!vec_vals.empty()) conf::set<size4>(set_str, size4(string2size_t(vec_vals[0]),
																		  string2size_t(vec_vals[1]),
																		  string2size_t(vec_vals[2]),
																		  string2size_t(vec_vals[3])));
				}
				break;
				case conf::CONF_TYPE::SSIZE_T: conf::set<ssize_t>(set_str, string2ssize_t(new_val)); break;
				case conf::CONF_TYPE::SSIZE2: {
					const vector<string> vec_vals(vec_arg_split(2));
					if(!vec_vals.empty()) conf::set<ssize2>(set_str, ssize2(string2ssize_t(vec_vals[0]),
																			string2ssize_t(vec_vals[1])));
				}
				break;
				case conf::CONF_TYPE::SSIZE3: {
					const vector<string> vec_vals(vec_arg_split(3));
					if(!vec_vals.empty()) conf::set<ssize3>(set_str, ssize3(string2ssize_t(vec_vals[0]),
																			string2ssize_t(vec_vals[1]),
																			string2ssize_t(vec_vals[2])));
				}
				break;
				case conf::CONF_TYPE::SSIZE4: {
					const vector<string> vec_vals(vec_arg_split(4));
					if(!vec_vals.empty()) conf::set<ssize4>(set_str, ssize4(string2ssize_t(vec_vals[0]),
																			string2ssize_t(vec_vals[1]),
																			string2ssize_t(vec_vals[2]),
																			string2ssize_t(vec_vals[3])));
				}
				break;
				case conf::CONF_TYPE::A2E_TEXTURE: break; // ignore this
			}
		}
		break;
		case COMMAND::GET: {
			if(tokens.size() < 2) {
				add_line("not enough parameters: get <setting>", false);
				break;
			}
			const string& set_str(tokens[1]);
			const auto settings = conf::get_settings();
			const auto set_iter = settings.find(set_str);
			if(set_iter == settings.end()) {
				add_line("unknown config setting", false);
				break;
			}
			stringstream get_str;
			get_str << set_str << ": ";
			switch(set_iter->second.first) {
				case conf::CONF_TYPE::STRING: get_str << conf::get<string>(set_str); break;
				case conf::CONF_TYPE::BOOL: get_str << conf::get<bool>(set_str); break;
				case conf::CONF_TYPE::FLOAT: get_str << conf::get<float>(set_str); break;
				case conf::CONF_TYPE::FLOAT2: get_str << conf::get<float2>(set_str); break;
				case conf::CONF_TYPE::FLOAT3: get_str << conf::get<float3>(set_str); break;
				case conf::CONF_TYPE::FLOAT4: get_str << conf::get<float4>(set_str); break;
				case conf::CONF_TYPE::SIZE_T: get_str << conf::get<size_t>(set_str); break;
				case conf::CONF_TYPE::SIZE2: get_str << conf::get<size2>(set_str); break;
				case conf::CONF_TYPE::SIZE3: get_str << conf::get<size3>(set_str); break;
				case conf::CONF_TYPE::SIZE4: get_str << conf::get<size4>(set_str); break;
				case conf::CONF_TYPE::SSIZE_T: get_str << conf::get<ssize_t>(set_str); break;
				case conf::CONF_TYPE::SSIZE2: get_str << conf::get<size2>(set_str); break;
				case conf::CONF_TYPE::SSIZE3: get_str << conf::get<size3>(set_str); break;
				case conf::CONF_TYPE::SSIZE4: get_str << conf::get<size4>(set_str); break;
				case conf::CONF_TYPE::A2E_TEXTURE: get_str << conf::get<a2e_texture>(set_str)->tex(); break;
			}
			add_line(get_str.str(), false);
		}
		break;
		case COMMAND::LIST: {
			string settings_str = "valid config settings: <i>";
			for(const auto& setting : conf::get_settings()) {
				settings_str += setting.first + "<" + conf::conf_type_to_string(setting.second.first) + "> ";
			}
			settings_str += "</i>";
			add_line(settings_str, false);
		}
		break;
		case COMMAND::HELP: {
			string help = "valid commands: <i>";
			for(const auto& vcmd : commands) {
				help += vcmd.first + " ";
			}
			help += "</i>";
			add_line(help, false);
		}
		break;
		case COMMAND::STATS: {
			add_line(u8"<b>#models</b>: " + size_t2string(sce->get_models().size()), false);
			add_line(u8"<b>#lights</b>: " + size_t2string(sce->get_lights().size()), false);
			add_line(u8"<b>#env probes</b>: " + size_t2string(sce->get_env_probes().size()), false);
			
			size_t active_rb = 0, active_drb = 0, active_sb = 0;
			pc->lock();
			for(const auto& rb : pc->get_rigid_bodies()) {
				if(rb->get_body()->getActivationState() == ACTIVE_TAG) {
					active_rb++;
				}
			}
			for(const auto& drb : pc->get_dynamic_rigid_bodies()) {
				if(drb->get_body()->getActivationState() == ACTIVE_TAG) {
					active_drb++;
				}
			}
			for(const auto& sb : pc->get_soft_bodies()) {
				if(sb->get_body()->getActivationState() == ACTIVE_TAG) {
					active_sb++;
				}
			}
			pc->unlock();
			
			add_line(u8"<b>#rigid bodies (active)</b>: " + size_t2string(pc->get_rigid_bodies().size()) + " (" + size_t2string(active_rb) + ")", false);
			add_line(u8"<b>#dynamic rigid bodies (active)</b>: " + size_t2string(pc->get_dynamic_rigid_bodies().size()) + " (" + size_t2string(active_drb) + ")", false);
			add_line(u8"<b>#soft bodies (active)</b>: " + size_t2string(pc->get_soft_bodies().size()) + " (" + size_t2string(active_sb) + ")", false);
			
			if(active_map != nullptr) {
				add_line(u8"<b>#triggers</b>: " + size_t2string(active_map->get_triggers().size()), false);
			}
		}
		break;
		case COMMAND::LOAD: {
			if(tokens.size() <= 1 || tokens[1].length() == 0) {
				a2e_error("no map name specified!");
				add_line(u8"<b>no map name specified!</b>", false);
				break;
			}
			
			e->acquire_gl_context();
			sb_map::change_map(tokens[1].find(".map") == string::npos ? (tokens[1]+".map") : tokens[1], GAME_STATUS::MAP_LOAD);
			add_line(active_map == nullptr ?
					 u8"<b>failed to load map!</b>" : u8"<b>map successfully loaded!</b>",
					 false);
			e->release_gl_context();
		}
		break;
		case COMMAND::SAVE: {
			if(active_map == nullptr) {
				a2e_error("couldn't save map, b/c none is active!");
				add_line(u8"<b>couldn't save map, b/c none is active!</b>", false);
				break;
			}
			
			string filename = active_map->get_filename();
			if(tokens.size() > 1 && tokens[1].length() > 0) {
				filename = tokens[1] + ".map";
			}
			a2e_debug("saving map as \"%s\" ...", filename);
			add_line(u8"saving map as \"" + filename + "\" ...", false);
			
			if(!map_storage::save(filename, *active_map)) {
				add_line(u8"<b>failed to save map!</b>", false);
				a2e_error("failed to save map!");
			}
			else {
				add_line(u8"<b>map successfully saved!</b>", false);
				a2e_debug("map successfully saved!");
			}
		}
		break;
		case COMMAND::EDITOR: {
			// open/close editor
			if(ed != nullptr) {
				ed->set_enabled(ed->is_enabled() ^ true);
				set_enabled(false); // disable console when opening/closing the editor
			}
		}
		break;
		case COMMAND::RUN: {
			if(tokens.size() != 3 ||
			   (tokens.size() == 0 && (tokens[1].empty() || tokens[2].empty()))) {
				add_line(u8"<b>usage: run <script> <function></b>", false);
				break;
			}
			
			script* scr = sh->load_script(tokens[1].find(".script") == string::npos ? tokens[1]+".script" : tokens[1]);
			if(scr == nullptr) {
				add_line(u8"<b>couldn't load script!</b>", false);
				break;
			}
			
			scr->execute(active_map, tokens[2]);
		}
		break;
		case COMMAND::INVALID:
			add_line(u8"invalid command: "+tokens[0], false);
			break;
	}
}

void sb_console::handle() {
	while(!command_queue.empty()) {
		const string cmd = command_queue.front();
		command_queue.pop_front();
		execute_cmd(cmd);
	}
	console_blink_handler(ui_cb_obj);
}
