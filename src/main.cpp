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

#include "main.h"

// global vars, you may change these
static const float3 cam_speed(0.02f, 0.2f, 0.002f);

//
static bool done = false;

int main(int argc, char* argv[]) {
	// initialize the engine
	e = new engine(argv[0], (const char*)"../data/");
	e->init();
	e->set_caption(APPLICATION_TITLE);
	e->acquire_gl_context();

	// init class pointers
	fio = e->get_file_io();
	eevt = e->get_event();
	t = e->get_texman();
	ocl = e->get_opencl();
	exts = e->get_ext();
	s = e->get_shader();
	sce = e->get_scene();
	ui = e->get_gui();
	fm = ui->get_font_manager();
	
	//
	conf::init();

	// load save-game handler
	save = new save_game();
	
	// initialize the camera
	cam = new camera(e);
	cam->set_position(-8.0f, -8.0f, -8.0f);
	cam->set_rotation(0.0f, 90.0f, 0.0f);
	cam->set_rotation_speed(150.0f);
	cam->set_cam_speed(cam_speed.x);
	
	if(conf::get<bool>("debug.no_menu")) {
		cam->set_mouse_input(true);
	}
	else {
		cam->set_mouse_input(false);
	}
	cam->set_keyboard_input(false);
	cam->set_wasd_input(false);
	
	// add custom shaders
	const string ar_shaders[4][2] = {
		{ "SB_GP_INSTANCED_MAP", "inferred/gp_instanced_map.a2eshd" },
		{ "SB_MP_INSTANCED_MAP", "inferred/mp_instanced_map.a2eshd" },
		{ "SB_EDITOR_SPRITES", "inferred/editor_sprites.a2eshd" },
		{ "SELECTED_BLOCK", "misc/selected_block.a2eshd" },
	};
	for(const auto& shd : ar_shaders) {
		if(!s->add_a2e_shader(shd[0], shd[1])) {
			a2e_error("couldn't add a2e-shader \"%s\"!", shd[1]);
			done = true;
		}
	}
	
	//
	menu = new menu_ui();

	// initialize audio
	ac = new audio_controller();
	
	// add event handlers
	event::handler event_handler_fnctr(&event_handler);
	eevt->add_event_handler(event_handler_fnctr,
							EVENT_TYPE::KEY_DOWN,
							EVENT_TYPE::KEY_UP,
							EVENT_TYPE::MOUSE_LEFT_DOWN,
							EVENT_TYPE::MOUSE_LEFT_UP,
							EVENT_TYPE::MOUSE_RIGHT_DOWN,
							EVENT_TYPE::MOUSE_RIGHT_UP,
							EVENT_TYPE::MOUSE_MIDDLE_DOWN,
							EVENT_TYPE::MOUSE_MIDDLE_UP,
							EVENT_TYPE::QUIT);
	
	// initialize physics
	pc = new physics_controller();

	// load/init stuff
	builtin_models::init();
	
	// init/create the particle manager if particles are enabled
	if(conf::get<bool>("gfx.particles")) {
		pm = new particle_manager(e);
		if(pm->get_manager() == nullptr) {
			conf::set<bool>("gfx.particles", false);
			a2e_error("disabling particle system support!");
			delete pm;
			pm = nullptr;
		}
		else {
			sce->add_particle_manager(pm);
		}
	}
	
	// initialize misc map/block classes
	bt = new block_textures();
	mr = new map_renderer();
	
	ge = new game(float3(0.0f));
	if(conf::get<bool>("debug.no_menu")) ge->set_enabled(true);
	
	ed = new editor(); // create after game!
	ed->set_enabled(false);
	
	console = new sb_console();
	
	// initialize script handler
	sh = new script_handler();
	
	// load the default or specified map
	if(conf::get<bool>("debug.no_menu")) {
		string level_filename = "intro.map";
		if(argc > 2 && !string(argv[1]).empty() && !string(argv[2]).empty()) {
			if(string(argv[1]) == "--level") {
				level_filename = argv[2];
			}
		}
		sb_map::change_map(level_filename, GAME_STATUS::MAP_LOAD);
	}
	
	// start physics sim
	pc->start_simulation();
	
	//
	sb_debug* debug_ui = new sb_debug();
	
	// initialization is done
	e->release_gl_context();

	// main loop
	while(!done) {
		// event handling
		eevt->handle_events();
		
		// stop drawing if window is inactive
		if(!(SDL_GetWindowFlags(e->get_window()) & SDL_WINDOW_INPUT_FOCUS)) {
			SDL_Delay(20);
			continue;
		}
		
		debug_ui->run();
		
		// set caption (app name and fps count)
		if(e->is_new_fps_count()) {
			static stringstream caption;
			caption << APPLICATION_TITLE << " | FPS: " << e->get_fps();
			if(ed != nullptr && ed->is_enabled()) {
				caption << " | Cam: " << float3(-*e->get_position());
				caption << " | " << float3(cam->get_rotation());
			}
			e->set_caption(caption.str().c_str());
			core::reset(caption);
		}

		e->start_draw();
		
		// command handling
		console->handle();
		
		// update model transformations
		gl_timer::mark("PHY_START");
		pc->update_models();
		gl_timer::mark("PHY_END");
		
		// map handling
		if(active_map != nullptr) {
			gl_timer::mark("MAP_START");
			// check for map change (for thread safety reasons this must be done here)
			const auto& map_change = active_map->is_map_change();
			if(map_change.first) {
				GAME_STATUS state = (map_change.second == nullptr ?
									 GAME_STATUS::DEATH : GAME_STATUS::MAP_LOAD);
				string destname = (map_change.second == nullptr ?
								   active_map->get_filename() : map_change.second->dst_map_name);
				sb_map::change_map(destname, state);
			}
			else active_map->run();
			gl_timer::mark("MAP_END");
		}

		cam->run();
		ac->run();
		if(ed != nullptr) ed->run();
		if(menu != nullptr) menu->handle();

		e->stop_draw();
	}
	
	// cleanup
	eevt->remove_event_handler(event_handler_fnctr);
	
	if(active_map != nullptr) {
		delete active_map;
		active_map = nullptr;
	}
	if(ge != nullptr) {
		delete ge;
		ge = nullptr;
	}

	save->dump_to_file();
	delete save;
	
	delete console;
	delete ed;
	ed = nullptr;
	delete mr;
	mr = nullptr;
	
	delete menu;
	delete ac;
	delete pc;
	if(sh != nullptr) delete sh;
	
	delete bt;
	
	delete debug_ui;
	
	conf::clear();
	if(pm != nullptr) delete pm;
	delete cam;
	delete e;

	return 0;
}

bool event_handler(EVENT_TYPE type, shared_ptr<event_object> obj) {
	//
	const pair<controls::ACTION, bool> action = controls::action_from_event(type, obj);
	switch(action.first) {
		case controls::ACTION::CONSOLE:
			if(!ed->is_enabled() && action.second) {
				e->acquire_gl_context();
				console->set_enabled(console->is_enabled() ^ true);
				e->release_gl_context();
			}
			break;
		default: break;
	}
	
	//
	if(type == EVENT_TYPE::KEY_DOWN) {
		const shared_ptr<key_down_event>& key_evt = (shared_ptr<key_down_event>&)obj;
		switch(key_evt->key) {
			case SDLK_LSHIFT:
			case SDLK_RSHIFT:
				cam->set_cam_speed(cam_speed.y);
				break;
			case SDLK_LCTRL:
			case SDLK_RCTRL:
				cam->set_cam_speed(cam_speed.z);
				break;
			default:
				return false;
		}
	}
	else if(type == EVENT_TYPE::KEY_UP) {
		const shared_ptr<key_up_event>& key_evt = (shared_ptr<key_up_event>&)obj;
		switch(key_evt->key) {
			case SDLK_LSHIFT:
			case SDLK_RSHIFT:
				cam->set_cam_speed(cam_speed.x);
				break;
			case SDLK_LCTRL:
			case SDLK_RCTRL:
				cam->set_cam_speed(cam_speed.x);
				break;
			case SDLK_ESCAPE:
				if(ge->is_enabled() || ed->is_enabled()) {
					menu->set_enabled(menu->is_enabled() ^ true);
				}
				else {
					// if neither the game nor the editor is active, simply go back to the main menu
					// also: no gui object may be active
					if(ui->get_active_object() == nullptr) {
						menu->back_to_main();
					}
					// if there is an active gui object, disable it
					else ui->set_active_object(nullptr);
				}
				break;
			default:
				return false;
		}
	}
	else if(type == EVENT_TYPE::QUIT) {
		done = true;
	}
	return true;
}
