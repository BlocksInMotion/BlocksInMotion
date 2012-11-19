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

#include "menu_ui.h"
#include "block_textures.h"
#include "sb_map.h"
#include "game.h"
#include "save.h"
#include "editor.h"
#include "audio_store.h"
#include "audio_background.h"
#include "audio_controller.h"
#include "new_map.h"
#include <gui/font_manager.h>
#include <gui/font.h>
#include <gui/objects/gui_window.h>
#include <gui/objects/gui_button.h>
#include <gui/objects/gui_text.h>
#include <gui/objects/gui_pop_up_button.h>
#include <gui/objects/gui_toggle_button.h>
#include <gui/objects/gui_slider.h>
#include <gui/objects/gui_list_box.h>
#include <gui/objects/gui_input_box.h>
#include <threading/task.h>
#include <scene/camera.h>

#if defined (__APPLE__)
#include <osx/osx_helper.h>
#endif

menu_ui::menu_ui() :
fnt(ui->get_font_manager()->get_font("SYSTEM_SANS_SERIF")),
evt_handler_fnctr(this, &menu_ui::event_handler)
{	
	eevt->add_internal_event_handler(evt_handler_fnctr,
									 EVENT_TYPE::AUDIO_STORE_LOAD,
									 EVENT_TYPE::KEY_DOWN,
									 EVENT_TYPE::MOUSE_LEFT_DOWN,
									 EVENT_TYPE::MOUSE_RIGHT_DOWN,
									 EVENT_TYPE::MOUSE_MIDDLE_DOWN);
	
	set_enabled(!conf::get<bool>("debug.no_menu"));
}

menu_ui::~menu_ui() {
	eevt->remove_event_handler(evt_handler_fnctr);
	ui->remove(menu_wnd);
	
	if(menu_music != nullptr) {
		ac->delete_audio_source("BG_MENU.0");
		menu_music = nullptr;
	}
}

bool menu_ui::event_handler(EVENT_TYPE type, shared_ptr<event_object> obj) {
	if(type == EVENT_TYPE::AUDIO_STORE_LOAD) {
		if(conf::get<bool>("debug.no_menu") || !enabled) return false;
		
		// if menu background music has loaded, play it
		const shared_ptr<audio_store_load_event>& load_evt = (shared_ptr<audio_store_load_event>&)obj;
		if(load_evt->identifier == "BG_MENU") {
			menu_music = ac->add_audio_background("BG_MENU", "0");
			if(menu_music != nullptr) {
				menu_music->play();
				menu_music->set_volume(1.0f);
			}
			return true;
		}
	}
	else if(active_menu == MENU_TYPE::CONTROLS && controls_ui.active_input != nullptr) {
		if(type == EVENT_TYPE::KEY_DOWN) {
			const shared_ptr<key_up_event>& key_evt = (shared_ptr<key_up_event>&)obj;
			// ignore escape and return keys
			if(key_evt->key != SDLK_ESCAPE &&
			   key_evt->key != SDLK_RETURN &&
			   key_evt->key != SDLK_RETURN2 &&
			   key_evt->key != SDLK_KP_ENTER) {
				controls::set(key_evt->key, controls_ui.active_action, controls_ui.active_side);
			}
			update_controls();
		}
		else {
			// check if the click is inside the input box
			const auto clicked_point = ((const mouse_event_base<EVENT_TYPE::__MOUSE_EVENT>&)*obj).position;
			if(controls_ui.active_input->should_handle_mouse_event(EVENT_TYPE::MOUSE_LEFT_DOWN, clicked_point)) {
				if(type == EVENT_TYPE::MOUSE_LEFT_DOWN) {
					controls::set((controls::action_event)controls::MOUSE_EVENT_TYPE::LEFT, controls_ui.active_action, controls_ui.active_side);
				}
				else if(type == EVENT_TYPE::MOUSE_RIGHT_DOWN) {
					controls::set((controls::action_event)controls::MOUSE_EVENT_TYPE::RIGHT, controls_ui.active_action, controls_ui.active_side);
				}
				else if(type == EVENT_TYPE::MOUSE_MIDDLE_DOWN) {
					controls::set((controls::action_event)controls::MOUSE_EVENT_TYPE::MIDDLE, controls_ui.active_action, controls_ui.active_side);
				}
				update_controls();
			}
		}
	}
	return false;
}

void menu_ui::set_enabled(const bool state) {
	e->acquire_gl_context();
	enabled = state;
	if(enabled) {
		if(ed != nullptr && ed->is_enabled()) {
			editor_was_active = true;
			ed->close_ui(); // close the editor ui when opening the menu ui
		}
		if(ge != nullptr && ge->is_enabled()) {
			game_was_active = true;
			ge->set_enabled(false);
		}
		
		if(menu_wnd == nullptr) {
			menu_wnd = ui->add<gui_window>(float2(1.0f), float2(0.0f));
		}
		next_menu = (unsigned int)MENU_TYPE::MAIN;
		e->set_cursor_visible(true);
		cam->set_mouse_input(false);
	}
	else {
		if(menu_wnd != nullptr) {
			ui->remove<gui_window>(menu_wnd);
			menu_wnd = nullptr;
		}
		next_menu = (unsigned int)MENU_TYPE::__MAX_MENU_TYPE;
		
		// stop menu background music
		if(menu_music != nullptr) {
			menu_music->stop();
		}
		
		// if the editor was active before the menu was opened, reopen the ui and keep the mouse cursor and 2d input active
		if(editor_was_active) {
			editor_was_active = false;
			ed->open_ui();
			e->set_cursor_visible(true);
			cam->set_mouse_input(false);
		}
		else {
			e->set_cursor_visible(false);
			cam->set_mouse_input(true);
		}
		
		//
		if(game_was_active) {
			game_was_active = false;
			ge->set_enabled(true);
		}
	}
	e->release_gl_context();
}

bool menu_ui::is_enabled() const {
	return enabled;
}

void menu_ui::back_to_main() {
	// check if we're already in the main menu and if the game was active
	if(active_menu == MENU_TYPE::MAIN && game_was_active) {
		// if so: disable the menu and reenable the game
		set_enabled(false);
		return;
	}
	active_menu = MENU_TYPE::__MAX_MENU_TYPE;
	next_menu = (unsigned int)MENU_TYPE::MAIN;
}

void menu_ui::handle() {
	const MENU_TYPE next = (MENU_TYPE)next_menu.load();
	if(next != active_menu) {
		open_menu(next);
	}
}

void menu_ui::open_menu(const MENU_TYPE type) {
	active_menu = type;
	if(menu_wnd == nullptr) return;
	
	//
	static constexpr float main_element_height = 0.1f;
	static constexpr float main_element_width = 0.4f;
	static constexpr float main_element_half_width = main_element_width * 0.5f;
	static a2e_constexpr float2 main_button_size(main_element_width, main_element_height);
	static constexpr float main_row_offset = main_element_height * 0.125f;
	static constexpr float max_main_height = ((float(MENU_TYPE::__MAX_MENU_TYPE) - 1.0f) *
											  (main_element_height + main_row_offset) - main_row_offset);
	
	//
	static constexpr float options_element_height = 0.04f;
	static a2e_constexpr array<const float, 5> options_column_offsets { { 0.1f, 0.225f, 0.5f, 0.65f, 0.8f } };
	static const float2 options_text_size(options_column_offsets[1] - options_column_offsets[0], options_element_height);
	static constexpr float options_row_offset = options_element_height * 0.15f;
	static constexpr size_t options_rows = 12;
	static constexpr float max_options_height = (float(options_rows) *
												 (options_element_height + options_row_offset) - options_row_offset);
	static constexpr float max_new_game_height = (3.0f *
												  (options_element_height + options_row_offset) - options_row_offset);
	
	//
	static constexpr float controls_element_height = 0.04f;
	static a2e_constexpr array<const float, 3> controls_column_offsets { { 0.2f, 0.4f, 0.6f } };
	static const float2 controls_text_size(controls_column_offsets[1] - controls_column_offsets[0], controls_element_height);
	static const float2 controls_input_size(0.19f, controls_element_height);
	static constexpr float controls_row_offset = controls_element_height * 0.15f;
	static constexpr size_t controls_rows = ((unsigned int)controls::ACTION::__MAX_ACTION) - 1;
	static constexpr float max_controls_height = (float(controls_rows) *
												  (controls_element_height + controls_row_offset) - controls_row_offset);
	
	//
	const float input_height = ((float(fnt->get_display_size()) + (gui_theme::point_to_pixel(1.5f) * 2.0f)) /
								float(menu_wnd->get_buffer()->height));
	const float new_row_offset = input_height * 0.15f;
	
	//
	float height = 0.0f;
	const auto next_height = [&height](const float& elem_height, const float& row_offset) -> float {
		const float ret { height };
		height += elem_height + row_offset;
		return ret;
	};
	
	menu_wnd->clear();
	switch(active_menu) {
		case MENU_TYPE::MAIN:
			height = 0.5f - max_main_height * 0.5f;
			main_ui.new_game_button = ui->add<gui_button>(main_button_size,
														  float2(0.5f - main_element_half_width, next_height(main_element_height, main_row_offset)));
			if(!game_was_active) {
				main_ui.load_button = ui->add<gui_button>(main_button_size,
														  float2(0.5f - main_element_half_width, next_height(main_element_height, main_row_offset)));
			}
			else {
				main_ui.load_button = ui->add<gui_button>(float2(main_button_size.x * 0.49f, main_button_size.y),
														  float2(0.5f + (main_button_size.x * 0.01f), height));
				main_ui.save_button = ui->add<gui_button>(float2(main_button_size.x * 0.49f, main_button_size.y),
														  float2(0.5f - main_element_half_width,
																 next_height(main_element_height, main_row_offset)));
			}
			main_ui.highscore_button = ui->add<gui_button>(main_button_size,
														   float2(0.5f - main_element_half_width, next_height(main_element_height, main_row_offset)));
			main_ui.options_button = ui->add<gui_button>(main_button_size,
														 float2(0.5f - main_element_half_width, next_height(main_element_height, main_row_offset)));
			main_ui.controls_button = ui->add<gui_button>(main_button_size,
														  float2(0.5f - main_element_half_width, next_height(main_element_height, main_row_offset)));
			main_ui.editor_button = ui->add<gui_button>(main_button_size,
														float2(0.5f - main_element_half_width, next_height(main_element_height, main_row_offset)));
			main_ui.credits_button = ui->add<gui_button>(main_button_size,
														 float2(0.5f - main_element_half_width, next_height(main_element_height, main_row_offset)));
			main_ui.quit_button = ui->add<gui_button>(main_button_size,
													  float2(0.5f - main_element_half_width, next_height(main_element_height, main_row_offset)));
			main_ui.new_game_button->set_label("<b>New Game</b>");
			main_ui.load_button->set_label("<b>Load</b>");
			if(game_was_active) main_ui.save_button->set_label("<b>Save</b>");
			main_ui.highscore_button->set_label("<b>Highscore</b>");
			main_ui.options_button->set_label("<b>Options</b>");
			main_ui.controls_button->set_label("<b>Controls</b>");
			main_ui.editor_button->set_label("<b>Editor</b>");
			main_ui.credits_button->set_label("<b>Credits</b>");
			main_ui.quit_button->set_label("<b>Quit</b>");
			menu_wnd->add_child(main_ui.new_game_button);
			menu_wnd->add_child(main_ui.load_button);
			if(game_was_active) menu_wnd->add_child(main_ui.save_button);
			menu_wnd->add_child(main_ui.highscore_button);
			menu_wnd->add_child(main_ui.options_button);
			menu_wnd->add_child(main_ui.controls_button);
			menu_wnd->add_child(main_ui.editor_button);
			menu_wnd->add_child(main_ui.credits_button);
			menu_wnd->add_child(main_ui.quit_button);
			
			main_ui.new_game_button->add_handler([&,this](GUI_EVENT gevt a2e_unused, gui_object& obj a2e_unused) {
				next_menu = (unsigned int)MENU_TYPE::NEW_GAME;
			}, GUI_EVENT::BUTTON_PRESS);
			
			main_ui.load_button->add_handler([&,this](GUI_EVENT gevt a2e_unused, gui_object& obj a2e_unused) {
				next_menu = (unsigned int)MENU_TYPE::LOAD;
			}, GUI_EVENT::BUTTON_PRESS);
			
			if(game_was_active) {
				main_ui.save_button->add_handler([&,this](GUI_EVENT gevt a2e_unused, gui_object& obj a2e_unused) {
					e->acquire_gl_context();
					save->save();
					save->dump_to_file();
					main_ui.save_button->set_label("Saved!");
					e->release_gl_context();
				}, GUI_EVENT::BUTTON_PRESS);
			}
			
			main_ui.highscore_button->add_handler([&,this](GUI_EVENT gevt a2e_unused, gui_object& obj a2e_unused) {
				next_menu = (unsigned int)MENU_TYPE::HIGHSCORE;
			}, GUI_EVENT::BUTTON_PRESS);
			
			main_ui.options_button->add_handler([&,this](GUI_EVENT gevt a2e_unused, gui_object& obj a2e_unused) {
				next_menu = (unsigned int)MENU_TYPE::OPTIONS;
			}, GUI_EVENT::BUTTON_PRESS);
			
			main_ui.controls_button->add_handler([&,this](GUI_EVENT gevt a2e_unused, gui_object& obj a2e_unused) {
				next_menu = (unsigned int)MENU_TYPE::CONTROLS;
			}, GUI_EVENT::BUTTON_PRESS);
			
			main_ui.editor_button->add_handler([&,this](GUI_EVENT gevt a2e_unused, gui_object& obj a2e_unused) {
				next_menu = (unsigned int)MENU_TYPE::EDITOR;
			}, GUI_EVENT::BUTTON_PRESS);
			
			main_ui.credits_button->add_handler([&,this](GUI_EVENT gevt a2e_unused, gui_object& obj a2e_unused) {
				next_menu = (unsigned int)MENU_TYPE::CREDITS;
			}, GUI_EVENT::BUTTON_PRESS);
			
			main_ui.quit_button->add_handler([this](GUI_EVENT gevt a2e_unused, gui_object& obj a2e_unused) {
				e->acquire_gl_context();
				eevt->add_event(EVENT_TYPE::QUIT, make_shared<quit_event>(SDL_GetTicks()));
				e->release_gl_context();
			}, GUI_EVENT::BUTTON_PRESS);
			
			break;
		case MENU_TYPE::NEW_GAME:
			height = 0.5f - max_new_game_height * 0.5f;
			new_game_ui.enter_name_text = ui->add<gui_text>(float2(0.8f, input_height),
															float2(0.1f, next_height(input_height, new_row_offset)));
			new_game_ui.name_input = ui->add<gui_input_box>(float2(0.8f, input_height),
															float2(0.1f, next_height(input_height, new_row_offset)));
			new_game_ui.start_button = ui->add<gui_button>(float2(0.8f, options_text_size.y),
														   float2(0.1f, next_height(options_text_size.y, new_row_offset)));
			new_game_ui.enter_name_text->set_label("<b>What is your name, robot?</b>");
			new_game_ui.enter_name_text->set_shade(true);
			new_game_ui.start_button->set_label("Start");
			menu_wnd->add_child(new_game_ui.enter_name_text);
			menu_wnd->add_child(new_game_ui.name_input);
			menu_wnd->add_child(new_game_ui.start_button);
			
			new_game_ui.start_button->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				const string& player_name = new_game_ui.name_input->get_input();
				if(player_name != "") {
					sb_map::change_map("intro.map", GAME_STATUS::MAP_LOAD);
					save->start_game(player_name);
					menu->set_enabled(false);
				}
				e->release_gl_context();
			}, GUI_EVENT::BUTTON_PRESS);
			break;
		case MENU_TYPE::LOAD: {
			load_ui.save_list = ui->add<gui_list_box>(float2(0.8f, 0.85f),
													  float2(0.1f, 0.05f));
			load_ui.load_button = ui->add<gui_button>(float2(0.8f, 0.05f - 0.0125f),
													  float2(0.1f, 0.9125f));
			load_ui.load_button->set_label("Load");
			menu_wnd->add_child(load_ui.save_list);
			menu_wnd->add_child(load_ui.load_button);
			
			// add saves to the list
			size_t save_counter = 0;
			for(const save_game::game_entry& save_entry : *save) {
				load_ui.save_list->add_item(size_t2string(save_counter), save_entry.save_string());
				save_counter++;
			}
			
			//
			load_ui.load_button->add_handler([&,this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				const size_t save_num(string2size_t(load_ui.save_list->get_selected_item()->first));
				auto iter = save->begin();
				advance(iter, save_num);
				sb_map::change_map(iter->map_name, GAME_STATUS::MAP_LOAD);
				save->load_game(*iter);
				menu->set_enabled(false);
				e->release_gl_context();
			}, GUI_EVENT::BUTTON_PRESS);
		}
		break;
		case MENU_TYPE::HIGHSCORE: {
			highscore_ui.highscore_list = ui->add<gui_list_box>(float2(0.8f, 0.9f),
																float2(0.1f, 0.05f));
			menu_wnd->add_child(highscore_ui.highscore_list);
			
			//
			size_t highscore_counter = 0;
			for(auto iter = save->begin_highscore(); iter != save->end_highscore(); iter++) {
				highscore_ui.highscore_list->add_item(size_t2string(highscore_counter), iter->stats_string());
				highscore_counter++;
			}
		}
		break;
		case MENU_TYPE::OPTIONS: {
			//
			xml::xml_doc& config_doc = e->get_config_doc();
			
			height = 0.5f - max_options_height * 0.5f;
			options_ui.screen_res_text = ui->add<gui_text>(options_text_size,
														   float2(options_column_offsets[0], next_height(options_element_height, options_row_offset)));
			next_height(options_element_height, options_row_offset); // -> for fullscreen/vsync toggle buttons
			options_ui.filtering_text = ui->add<gui_text>(options_text_size,
														  float2(options_column_offsets[0], next_height(options_element_height, options_row_offset)));
			options_ui.aniso_filtering_text = ui->add<gui_text>(options_text_size,
																float2(options_column_offsets[0], next_height(options_element_height, options_row_offset)));
			options_ui.anti_aliasing_text = ui->add<gui_text>(options_text_size,
															  float2(options_column_offsets[0], next_height(options_element_height, options_row_offset)));
			options_ui.tex_quality_text = ui->add<gui_text>(options_text_size,
															float2(options_column_offsets[0], next_height(options_element_height, options_row_offset)));
			options_ui.music_volume_text = ui->add<gui_text>(options_text_size,
															 float2(options_column_offsets[0], next_height(options_element_height, options_row_offset)));
			options_ui.sound_volume_text = ui->add<gui_text>(options_text_size,
															 float2(options_column_offsets[0], next_height(options_element_height, options_row_offset)));
			options_ui.fov_text = ui->add<gui_text>(options_text_size,
													float2(options_column_offsets[0], next_height(options_element_height, options_row_offset)));
			options_ui.upscaling_text = ui->add<gui_text>(options_text_size,
														  float2(options_column_offsets[0], next_height(options_element_height, options_row_offset)));
			options_ui.cl_platform_text = ui->add<gui_text>(options_text_size,
															float2(options_column_offsets[0], next_height(options_element_height, options_row_offset)));
			options_ui.cl_restrict_text = ui->add<gui_text>(options_text_size,
															float2(options_column_offsets[0], next_height(options_element_height, options_row_offset)));
			options_ui.screen_res_text->set_label("<b>Resolution</b>");
			options_ui.filtering_text->set_label("<b>Filtering</b>");
			options_ui.aniso_filtering_text->set_label("<b>Anisotropic</b>");
			options_ui.anti_aliasing_text->set_label("<b>Anti-Aliasing</b>");
			options_ui.tex_quality_text->set_label("<b>Texture Quality</b>");
			options_ui.music_volume_text->set_label("<b>Music Volume</b>");
			options_ui.sound_volume_text->set_label("<b>Sound Volume</b>");
			options_ui.fov_text->set_label("<b>FOV</b>");
			options_ui.upscaling_text->set_label("<b>Upscaling</b>");
			options_ui.cl_platform_text->set_label("<b>OpenCL Platform</b>");
			options_ui.cl_restrict_text->set_label("<b>OpenCL Devices</b>");
			options_ui.screen_res_text->set_shade(true);
			options_ui.filtering_text->set_shade(true);
			options_ui.aniso_filtering_text->set_shade(true);
			options_ui.anti_aliasing_text->set_shade(true);
			options_ui.tex_quality_text->set_shade(true);
			options_ui.music_volume_text->set_shade(true);
			options_ui.sound_volume_text->set_shade(true);
			options_ui.fov_text->set_shade(true);
			options_ui.upscaling_text->set_shade(true);
			options_ui.cl_platform_text->set_shade(true);
			options_ui.cl_restrict_text->set_shade(true);
			menu_wnd->add_child(options_ui.screen_res_text);
			menu_wnd->add_child(options_ui.filtering_text);
			menu_wnd->add_child(options_ui.aniso_filtering_text);
			menu_wnd->add_child(options_ui.anti_aliasing_text);
			menu_wnd->add_child(options_ui.tex_quality_text);
			menu_wnd->add_child(options_ui.music_volume_text);
			menu_wnd->add_child(options_ui.sound_volume_text);
			menu_wnd->add_child(options_ui.fov_text);
			menu_wnd->add_child(options_ui.upscaling_text);
			menu_wnd->add_child(options_ui.cl_platform_text);
			menu_wnd->add_child(options_ui.cl_restrict_text);
			
			height = 0.5f - max_options_height * 0.5f;
			// resolution
			options_ui.screen_res_popup = ui->add<gui_pop_up_button>(float2(options_column_offsets[2] - options_column_offsets[1], options_element_height),
																	 float2(options_column_offsets[1], next_height(options_element_height, options_row_offset)));
			static const vector<pair<uint2, string>> screen_resolutions {
				{
					{ uint2(1280, 720), "720p" }, // 720p
					{ uint2(1920, 1080), "1080p" }, // 1080p
					
					// -> http://store.steampowered.com/hwsurvey
					// this should cover most resolutions
					{ uint2(1024, 768), "4:3" },
					{ uint2(1152, 864), "4:3" },
					{ uint2(1280, 768), "5:3" },
					{ uint2(1280, 960), "4:3" },
					{ uint2(1280, 1024), "5:4" },
					{ uint2(1360, 768), "16:9" },
					{ uint2(1366, 768), "16:9" },
					{ uint2(1600, 900), "16:9" },
					{ uint2(2560, 1440), "16:9" },
					{ uint2(1280, 800), "16:10" },
					{ uint2(1440, 900), "16:10" },
					{ uint2(1680, 1050), "16:10" },
					{ uint2(1920, 1200), "16:10" },
					{ uint2(2880, 1800), "16:10" },
				}
			};
			
			// get current screen resolution
			SDL_Rect display_bounds;
			SDL_GetDisplayBounds(0, &display_bounds);
			uint2 display_res(display_bounds.w, display_bounds.h);
			uint2 active_res(e->get_width(), e->get_height());
#if defined(__APPLE__)
			const float scale_factor = osx_helper::get_scale_factor(e->get_window());
			display_res = float2(display_res) * scale_factor;
			active_res = float2(active_res) / scale_factor;
#endif
			bool display_res_in_list = false, active_res_in_list = false;
			for(const auto& res : screen_resolutions) {
				if(res.first.x == display_res.x && res.first.y == display_res.y) {
					display_res_in_list = true;
				}
				if(res.first.x == active_res.x && res.first.y == active_res.y) {
					active_res_in_list = true;
				}
			}
			
			for(const auto& res : screen_resolutions) {
				options_ui.screen_res_popup->add_item(uint2string(res.first.x)+"_"+uint2string(res.first.y), uint2string(res.first.x)+" x "+uint2string(res.first.y)+" ("+res.second+")");
			}
			if(!display_res_in_list) {
				options_ui.screen_res_popup->add_item(uint2string(display_res.x)+"_"+uint2string(display_res.y), uint2string(display_res.x)+" x "+uint2string(display_res.y)+" (display)");
			}
			if(!active_res_in_list) {
				options_ui.screen_res_popup->add_item(uint2string(active_res.x)+"_"+uint2string(active_res.y), uint2string(active_res.x)+" x "+uint2string(active_res.y)+" (custom)");
			}
			options_ui.screen_res_popup->set_selected_item(uint2string(active_res.x)+"_"+uint2string(active_res.y));
			menu_wnd->add_child(options_ui.screen_res_popup);
			
			// fullscreen + vsync toggle
			const float max_width = options_column_offsets[2] - options_column_offsets[1];
			options_ui.fullscreen_toggle = ui->add<gui_toggle_button>(float2(max_width * 0.49f, options_element_height),
																	  float2(options_column_offsets[1], height));
			options_ui.vsync_toggle = ui->add<gui_toggle_button>(float2(max_width * 0.49f, options_element_height),
																 float2(options_column_offsets[1] + max_width * 0.51f, next_height(options_element_height, options_row_offset)));
			options_ui.fullscreen_toggle->set_label("Fullscreen", "Windowed");
			options_ui.vsync_toggle->set_label("VSync", "no VSync");
			menu_wnd->add_child(options_ui.fullscreen_toggle);
			menu_wnd->add_child(options_ui.vsync_toggle);
			
			options_ui.fullscreen_toggle->set_toggled(e->get_fullscreen());
			options_ui.vsync_toggle->set_toggled(e->get_vsync());
			
			// filtering
			options_ui.filtering_popup = ui->add<gui_pop_up_button>(float2(options_column_offsets[2] - options_column_offsets[1], options_element_height),
																	float2(options_column_offsets[1], next_height(options_element_height, options_row_offset)));
			options_ui.filtering_aniso_popup = ui->add<gui_pop_up_button>(float2(options_column_offsets[2] - options_column_offsets[1], options_element_height),
																		  float2(options_column_offsets[1], next_height(options_element_height, options_row_offset)));
			
			options_ui.filtering_popup->add_item(uint2string((unsigned int)TEXTURE_FILTERING::POINT), "Point / Nearest");
			options_ui.filtering_popup->add_item(uint2string((unsigned int)TEXTURE_FILTERING::LINEAR), "Linear");
			options_ui.filtering_popup->add_item(uint2string((unsigned int)TEXTURE_FILTERING::BILINEAR), "Bilinear");
			options_ui.filtering_popup->add_item(uint2string((unsigned int)TEXTURE_FILTERING::TRILINEAR), "Trilinear");
			
			const auto max_aniso = exts->get_max_anisotropic_filtering();
			options_ui.filtering_aniso_popup->add_item("0", "disabled");
			for(unsigned int cur_aniso = 1; cur_aniso <= max_aniso; cur_aniso <<= 1) {
				const string aniso_str(uint2string(cur_aniso));
				options_ui.filtering_aniso_popup->add_item(aniso_str, aniso_str+"x");
			}
			
			menu_wnd->add_child(options_ui.filtering_popup);
			menu_wnd->add_child(options_ui.filtering_aniso_popup);
			
			options_ui.filtering_popup->set_selected_item((unsigned int)e->get_filtering());
			options_ui.filtering_aniso_popup->set_selected_item(size_t2string(e->get_anisotropic()));
			
			// anti-aliasing
			options_ui.anti_aliasing_popup = ui->add<gui_pop_up_button>(float2(options_column_offsets[2] - options_column_offsets[1], options_element_height),
																		float2(options_column_offsets[1], next_height(options_element_height, options_row_offset)));
			options_ui.anti_aliasing_popup->add_item(uint2string((unsigned int)rtt::TEXTURE_ANTI_ALIASING::NONE), "None");
			options_ui.anti_aliasing_popup->add_item(uint2string((unsigned int)rtt::TEXTURE_ANTI_ALIASING::FXAA), "FXAA");
			options_ui.anti_aliasing_popup->add_item(uint2string((unsigned int)rtt::TEXTURE_ANTI_ALIASING::SSAA_4_3_FXAA), "SSAA 4/3x + FXAA");
			options_ui.anti_aliasing_popup->add_item(uint2string((unsigned int)rtt::TEXTURE_ANTI_ALIASING::SSAA_2), "SSAA 2x");
			options_ui.anti_aliasing_popup->add_item(uint2string((unsigned int)rtt::TEXTURE_ANTI_ALIASING::SSAA_2_FXAA), "SSAA 2x + FXAA");
			
			menu_wnd->add_child(options_ui.anti_aliasing_popup);
			
			options_ui.anti_aliasing_popup->set_selected_item(uint2string((unsigned int)e->get_anti_aliasing()));
			
			// tex-quality
			options_ui.tex_quality_popup = ui->add<gui_pop_up_button>(float2(options_column_offsets[2] - options_column_offsets[1], options_element_height),
																	  float2(options_column_offsets[1], next_height(options_element_height, options_row_offset)));
			options_ui.tex_quality_popup->add_item("low", "Low (128*128px)");
			options_ui.tex_quality_popup->add_item("mid", "Medium (256*256px)");
			options_ui.tex_quality_popup->add_item("high", "High (512*512px)");
			options_ui.tex_quality_popup->add_item("ultra", "Ultra (1024*1024px)");
			menu_wnd->add_child(options_ui.tex_quality_popup);
			
			options_ui.tex_quality_popup->set_selected_item(conf::get<string>("gfx.texture_quality"));
			
			// music_volume slider
			const float slider_width = (options_column_offsets[2] - options_column_offsets[1]) * 0.85f;
			const float slider_value_offset = (options_column_offsets[2] - options_column_offsets[1]) * 0.025f;
			const float slider_value_width = (options_column_offsets[2] - options_column_offsets[1]) * 0.125f;
			options_ui.music_volume_slider = ui->add<gui_slider>(float2(slider_width, options_element_height),
																 float2(options_column_offsets[1], height));
			options_ui.music_volume_value = ui->add<gui_text>(float2(slider_value_width, options_element_height),
															  float2(options_column_offsets[1] + slider_width + slider_value_offset, next_height(options_element_height, options_row_offset)));
			options_ui.music_volume_value->set_label("");
			options_ui.music_volume_value->set_shade(true);
			menu_wnd->add_child(options_ui.music_volume_slider);
			menu_wnd->add_child(options_ui.music_volume_value);
			
			// sound_volume slider
			options_ui.sound_volume_slider = ui->add<gui_slider>(float2(slider_width, options_element_height),
																 float2(options_column_offsets[1], height));
			options_ui.sound_volume_value = ui->add<gui_text>(float2(slider_value_width, options_element_height),
															  float2(options_column_offsets[1] + slider_width + slider_value_offset, next_height(options_element_height, options_row_offset)));
			options_ui.sound_volume_value->set_label("");
			options_ui.sound_volume_value->set_shade(true);
			menu_wnd->add_child(options_ui.sound_volume_slider);
			menu_wnd->add_child(options_ui.sound_volume_value);
			
			// fov slider
			options_ui.fov_slider = ui->add<gui_slider>(float2(slider_width, options_element_height),
														float2(options_column_offsets[1], height));
			options_ui.fov_value = ui->add<gui_text>(float2(slider_value_width, options_element_height),
													 float2(options_column_offsets[1] + slider_width + slider_value_offset, next_height(options_element_height, options_row_offset)));
			options_ui.fov_value->set_label("");
			options_ui.fov_value->set_shade(true);
			menu_wnd->add_child(options_ui.fov_slider);
			menu_wnd->add_child(options_ui.fov_value);
			
			// upscaling
			options_ui.upscaling_popup = ui->add<gui_pop_up_button>(float2(options_column_offsets[2] - options_column_offsets[1], options_element_height),
																	float2(options_column_offsets[1], next_height(options_element_height, options_row_offset)));
			options_ui.upscaling_popup->add_item("1.0", "1x");
			options_ui.upscaling_popup->add_item("1.3", "4/3x");
			options_ui.upscaling_popup->add_item("1.5", "1.5x");
			options_ui.upscaling_popup->add_item("2.0", "2x");
			menu_wnd->add_child(options_ui.upscaling_popup);
			
			const string upscaling_str(float2string(e->get_upscaling()));
			options_ui.upscaling_popup->set_selected_item(upscaling_str.length() > 3 ? upscaling_str.substr(0, 3) : upscaling_str);
			
			// cl platform
			options_ui.cl_platform_popup = ui->add<gui_pop_up_button>(float2(options_column_offsets[2] - options_column_offsets[1], options_element_height),
																	  float2(options_column_offsets[1], next_height(options_element_height, options_row_offset)));
			const auto platforms = opencl::get_platforms();
			for(const auto& platform : platforms) {
				options_ui.cl_platform_popup->add_item(platform.second,
													   (platform.second == "cuda" ? "CUDA" : platform.second) +
													   " (" + opencl_base::platform_vendor_to_str(platform.first) + ")");
			}
			menu_wnd->add_child(options_ui.cl_platform_popup);
			options_ui.cl_platform_popup->set_selected_item(config_doc.get<string>("config.opencl.platform", "0"));
			
			// cl devices / restrict
			options_ui.cl_restrict_popup = ui->add<gui_pop_up_button>(float2(options_column_offsets[2] - options_column_offsets[1], options_element_height),
																	  float2(options_column_offsets[1], next_height(options_element_height, options_row_offset)));
			options_ui.cl_restrict_popup->add_item("0", "All");
			options_ui.cl_restrict_popup->add_item("cpu", "CPU only");
			options_ui.cl_restrict_popup->add_item("gpu", "GPU only");
			menu_wnd->add_child(options_ui.cl_restrict_popup);
			
			const string cl_restrict(config_doc.get<string>("config.opencl.restrict", ""));
			if(cl_restrict == "CPU") options_ui.cl_restrict_popup->set_selected_item("cpu");
			else if(cl_restrict == "GPU") options_ui.cl_restrict_popup->set_selected_item("gpu");
			else options_ui.cl_restrict_popup->set_selected_item("0");
			
			// event handling
			options_ui.screen_res_popup->add_handler([&config_doc,this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				const auto res_tokens = core::tokenize(options_ui.screen_res_popup->get_selected_item()->first, '_');
				uint2 new_screen_res(string2uint(res_tokens[0]), string2uint(res_tokens[1]));
				e->set_screen_size(new_screen_res);
				config_doc.set<size_t>("config.screen.width", e->get_width());
				config_doc.set<size_t>("config.screen.height", e->get_height());
				save_config();
				e->release_gl_context();
			}, GUI_EVENT::POP_UP_BUTTON_SELECT);
			
			options_ui.fullscreen_toggle->add_handler([&config_doc,this](GUI_EVENT gevt, gui_object&) {
				e->acquire_gl_context();
				e->set_fullscreen(gevt == GUI_EVENT::TOGGLE_BUTTON_ACTIVATION);
				config_doc.set<bool>("config.screen.fullscreen", e->get_fullscreen());
				save_config();
				e->release_gl_context();
			}, GUI_EVENT::TOGGLE_BUTTON_ACTIVATION, GUI_EVENT::TOGGLE_BUTTON_DEACTIVATION);
			
			options_ui.vsync_toggle->add_handler([&config_doc,this](GUI_EVENT gevt, gui_object&) {
				e->acquire_gl_context();
				e->set_vsync(gevt == GUI_EVENT::TOGGLE_BUTTON_ACTIVATION);
				config_doc.set<bool>("config.screen.vsync", e->get_vsync());
				save_config();
				e->release_gl_context();
			}, GUI_EVENT::TOGGLE_BUTTON_ACTIVATION, GUI_EVENT::TOGGLE_BUTTON_DEACTIVATION);
			
			const auto reload_block_textures = []() {
				if(bt != nullptr) delete bt;
				bt = new block_textures();
			};
			options_ui.filtering_popup->add_handler([&config_doc,&reload_block_textures,this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				e->set_filtering((TEXTURE_FILTERING)string2uint(options_ui.filtering_popup->get_selected_item()->first));
				string filtering_str = "POINT";
				switch(e->get_filtering()) {
					case TEXTURE_FILTERING::LINEAR:
						filtering_str = "LINEAR";
						break;
					case TEXTURE_FILTERING::BILINEAR:
						filtering_str = "BILINEAR";
						break;
					case TEXTURE_FILTERING::TRILINEAR:
						filtering_str = "TRILINEAR";
						break;
					case TEXTURE_FILTERING::POINT:
					default:
						break;
				}
				config_doc.set<string>("config.graphic.filtering", filtering_str);
				reload_block_textures();
				save_config();
				e->release_gl_context();
			}, GUI_EVENT::POP_UP_BUTTON_SELECT);
			
			options_ui.filtering_aniso_popup->add_handler([&config_doc,&reload_block_textures,this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				e->set_anisotropic(string2size_t(options_ui.filtering_aniso_popup->get_selected_item()->first));
				config_doc.set<size_t>("config.graphic.anisotropic", e->get_anisotropic());
				reload_block_textures();
				save_config();
				e->release_gl_context();
			}, GUI_EVENT::POP_UP_BUTTON_SELECT);
			
			options_ui.anti_aliasing_popup->add_handler([&config_doc,this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				e->set_anti_aliasing((rtt::TEXTURE_ANTI_ALIASING)string2uint(options_ui.anti_aliasing_popup->get_selected_item()->first));
				config_doc.set<string>("config.graphic.anti_aliasing",
									   rtt::TEXTURE_ANTI_ALIASING_STR[(unsigned int)e->get_anti_aliasing()]);
				save_config();
				e->release_gl_context();
			}, GUI_EVENT::POP_UP_BUTTON_SELECT);
			
			options_ui.tex_quality_popup->add_handler([&config_doc,&reload_block_textures,this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				conf::set<string>("gfx.texture_quality", options_ui.tex_quality_popup->get_selected_item()->first);
				config_doc.set<string>("config.bim.gfx.texture.quality",
									   conf::get<string>("gfx.texture_quality"));
				reload_block_textures();
				save_config();
				e->release_gl_context();
			}, GUI_EVENT::POP_UP_BUTTON_SELECT);
			
			options_ui.upscaling_popup->add_handler([&config_doc,this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				e->set_upscaling(string2float(options_ui.upscaling_popup->get_selected_item()->first));
				config_doc.set<string>("config.inferred.upscaling",
									   options_ui.upscaling_popup->get_selected_item()->first);
				save_config();
				e->release_gl_context();
			}, GUI_EVENT::POP_UP_BUTTON_SELECT);
			
			options_ui.cl_platform_popup->add_handler([&config_doc,this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				const string& selected_platform(options_ui.cl_platform_popup->get_selected_item()->first);
				config_doc.set<string>("config.opencl.platform", selected_platform);
				save_config();
				e->release_gl_context();
			}, GUI_EVENT::POP_UP_BUTTON_SELECT);
			
			options_ui.cl_restrict_popup->add_handler([&config_doc,this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				const string& selected_option(options_ui.cl_restrict_popup->get_selected_item()->first);
				string restrict_option = "";
				if(selected_option == "cpu") restrict_option = "CPU";
				else if(selected_option == "gpu") restrict_option = "GPU";
				config_doc.set<string>("config.opencl.restrict", restrict_option);
				save_config();
				e->release_gl_context();
			}, GUI_EVENT::POP_UP_BUTTON_SELECT);
			
			const auto trim_float = [](const float& value) -> string {
				const string str = float2string(roundf(value));
				const auto point_pos = str.find(".");
				if(point_pos == string::npos) return str;
				return str.substr(0, point_pos);
			};
			options_ui.music_volume_slider->add_handler([&config_doc,&trim_float,this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				const float volume = roundf(options_ui.music_volume_slider->get_knob_position() * 100.0f);
				const float conf_volume = volume / 100.0f;
				if(conf::get<float>("volume.music") != conf_volume) {
					conf::set<float>("volume.music", conf_volume);
				}
				config_doc.set<float>("config.bim.volume.music", volume / 100.0f);
				save_config();
				options_ui.music_volume_value->set_label(trim_float(volume)+"%");
				e->release_gl_context();
			}, GUI_EVENT::SLIDER_MOVE);
			
			options_ui.sound_volume_slider->add_handler([&config_doc,&trim_float,this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				const float volume = roundf(options_ui.sound_volume_slider->get_knob_position() * 100.0f);
				const float conf_volume = volume / 100.0f;
				if(conf::get<float>("volume.sound") != conf_volume) {
					conf::set<float>("volume.sound", conf_volume);
				}
				config_doc.set<float>("config.bim.volume.sound", conf_volume);
				save_config();
				options_ui.sound_volume_value->set_label(trim_float(volume)+"%");
				e->release_gl_context();
			}, GUI_EVENT::SLIDER_MOVE);
			
			static a2e_constexpr float2 fov_minmax(60.0f, 120.0f);
			options_ui.fov_slider->add_handler([&config_doc,&trim_float,this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				const float fov = floorf(options_ui.fov_slider->get_knob_position() * (fov_minmax.y - fov_minmax.x) + fov_minmax.x);
				e->set_fov(fov);
				const string fov_str(trim_float(fov));
				config_doc.set<string>("config.projection.fov", fov_str);
				save_config();
				options_ui.fov_value->set_label(fov_str+"°");
				e->release_gl_context();
			}, GUI_EVENT::SLIDER_MOVE);
			options_ui.music_volume_slider->set_knob_position(conf::get<float>("volume.music"));
			options_ui.sound_volume_slider->set_knob_position(conf::get<float>("volume.sound"));
			options_ui.fov_slider->set_knob_position((e->get_fov() - fov_minmax.x) / (fov_minmax.y - fov_minmax.x));
		}
		break;
		case MENU_TYPE::CONTROLS: {
			height = 0.5f - max_controls_height * 0.5f;
			controls_ui.active_input = nullptr;
			controls_ui.active_action = controls::ACTION::NONE;
			controls_ui.active_side = true;
			unsigned int action_enum = (unsigned int)controls::ACTION::NONE;
			for(auto& action : controls_ui.actions) {
				action_enum++;
				action.desc = ui->add<gui_text>(controls_text_size,
												float2(controls_column_offsets[0], height));
				action.mapping[0] = ui->add<gui_input_box>(controls_input_size,
														   float2(controls_column_offsets[1], height));
				action.mapping[1] = ui->add<gui_input_box>(controls_input_size,
														   float2(controls_column_offsets[2], height));
				action.desc->set_label("<b>"+controls::name((controls::ACTION)action_enum)+"</b>");
				action.desc->set_shade(true);
				menu_wnd->add_child(action.desc);
				menu_wnd->add_child(action.mapping[0]);
				menu_wnd->add_child(action.mapping[1]);
				next_height(controls_element_height, controls_row_offset);
				
				//
				for(size_t i = 0; i < 2; i++) {
					action.mapping[i]->add_handler([i, action_enum, this](GUI_EVENT, gui_object& obj) {
						if(obj.is_active()) {
							controls_ui.active_input = (gui_input_box*)&obj;
							controls_ui.active_action = (controls::ACTION)action_enum;
							controls_ui.active_side = (i == 0);
						}
						else { // deactivation
							if(controls_ui.active_input == &obj) {
								controls_ui.active_input = nullptr;
								controls_ui.active_action = controls::ACTION::NONE;
								controls_ui.active_side = true;
							}
						}
					}, GUI_EVENT::INPUT_BOX_ACTIVATION, GUI_EVENT::INPUT_BOX_DEACTIVATION);
					action.mapping[i]->add_handler([action_enum, this](GUI_EVENT, gui_object&) {
						e->acquire_gl_context();
						update_controls();
						e->release_gl_context();
					}, GUI_EVENT::INPUT_BOX_ENTER, GUI_EVENT::INPUT_BOX_INPUT);
				}
			}
			update_controls();
		}
		break;
		case MENU_TYPE::EDITOR: {
			edit_ui.map_list = ui->add<gui_list_box>(float2(0.8f, 0.85f),
													 float2(0.1f, 0.05f));
			edit_ui.edit_button = ui->add<gui_button>(float2(0.4f, 0.05f - 0.0125f),
													  float2(0.1f, 0.9125f));
			edit_ui.new_button = ui->add<gui_button>(float2(0.4f, 0.05f - 0.0125f),
													  float2(0.5f, 0.9125f));
			edit_ui.edit_button->set_label("Edit");
			edit_ui.new_button->set_label("New");
			menu_wnd->add_child(edit_ui.map_list);
			menu_wnd->add_child(edit_ui.edit_button);
			menu_wnd->add_child(edit_ui.new_button);
			
			// add every map to the list
			const auto map_list = core::get_file_list(e->data_path("maps/"), "map");
			for(const auto& map_filename : map_list) {
				string label = map_filename.first;
				file_io file(e->data_path("maps/"+map_filename.first), file_io::OPEN_TYPE::READ_BINARY);
				if(file.is_open()) {
					file.get_uint(); // magic
					file.get_uint(); // version
					string map_name = "";
					file.get_terminated_block(map_name, 0);
					label += " (\"" + map_name + "\")";
					file.close();
				}
				edit_ui.map_list->add_item(map_filename.first, label);
			}
			
			//
			edit_ui.edit_button->add_handler([&,this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				sb_map::change_map(edit_ui.map_list->get_selected_item()->first, GAME_STATUS::MAP_LOAD);
				ge->set_enabled(false);
				menu->set_enabled(false);
				ed->set_enabled(true);
				e->release_gl_context();
			}, GUI_EVENT::BUTTON_PRESS);
			
			edit_ui.new_button->add_handler([&,this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				next_menu = (unsigned int)MENU_TYPE::NEW_MAP;
				e->release_gl_context();
			}, GUI_EVENT::BUTTON_PRESS);
		}
		break;
		case MENU_TYPE::NEW_MAP: {
			height = 0.5f - max_new_game_height * 0.5f;
			new_map_ui.enter_map_name_text = ui->add<gui_text>(float2(0.8f, input_height),
															   float2(0.1f, next_height(input_height, new_row_offset)));
			new_map_ui.map_name_input = ui->add<gui_input_box>(float2(0.8f, input_height),
															   float2(0.1f, next_height(input_height, new_row_offset)));
			new_map_ui.create_button = ui->add<gui_button>(float2(0.8f, options_text_size.y),
														   float2(0.1f, next_height(options_text_size.y, new_row_offset)));
			new_map_ui.enter_map_name_text->set_label("<b>Map filename:</b>");
			new_map_ui.enter_map_name_text->set_shade(true);
			new_map_ui.create_button->set_label("Create");
			menu_wnd->add_child(new_map_ui.enter_map_name_text);
			menu_wnd->add_child(new_map_ui.map_name_input);
			menu_wnd->add_child(new_map_ui.create_button);
			
			new_map_ui.create_button->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				string map_name = new_map_ui.map_name_input->get_input();
				
				// append .map if it's not there already
				if(map_name.size() < 4 || map_name.rfind(".map") != map_name.size()-4) {
					map_name += ".map";
				}
				
				if(map_name != "") {
					if(file_io::is_file(e->data_path("maps/"+map_name))) {
						// map already exists
						a2e_error("a map with the filename \"%s\" already exists!", map_name);
						new_map_ui.enter_map_name_text->set_label("<b>Map filename:</b> Map \""+map_name+"\" already exists!");
					}
					else {
						//
						file_io new_map(e->data_path("maps/"+map_name), file_io::OPEN_TYPE::WRITE_BINARY);
						new_map.write_block((const char*)&new_map_data[0], sizeof(new_map_data));
						new_map.close();
						
						//
						sb_map::change_map(map_name, GAME_STATUS::MAP_LOAD);
						ge->set_enabled(false);
						menu->set_enabled(false);
						ed->set_enabled(true);
					}
				}
				e->release_gl_context();
			}, GUI_EVENT::BUTTON_PRESS);
		}
		break;
		case MENU_TYPE::CREDITS: {
			static const vector<string> bim_credits {
				{
					u8"<b>Blocks In Motion</b>",
					u8"Florian Ziesche",
					u8"Yannic Haupenthal",
					u8"",
				}
			};
			
			static const vector<string> lib_credits {
				{
					u8"<b>Libraries</b>",
					u8"a2elight (https://github.com/a2flo/a2elight)",
					u8"SDL2 and SDL2_image (http://www.libsdl.org)",
					u8"Bullet Physics (http://www.bulletphysics.org)",
					u8"libpng (http://www.libpng.org)",
					u8"zlib (http://zlib.net)",
					u8"libxml2 (http://www.xmlsoft.org)",
					u8"premake4 (http://www.industriousone.com/premake)",
					u8"MinGW-w64/MSYS environment (http://mingw-w64.sourceforge.net)",
					u8"FreeType2 (http://www.freetype.org)",
					u8"OpenAL (http://www.openal.org)",
					u8"OpenALSoft (http://kcat.strangesoft.net/openal.html)",
					u8"mpg123 (http://www.mpg123.de)",
					u8"Bitstream DejaVu Fonts (http://www.dejavu-fonts.org)",
					u8"clang/libc++ (http://www.llvm.org)",
					u8"gcc/libstdc++ (http://gcc.gnu.org)",
					u8"Khronos OpenGL and OpenCL (http://www.khronos.org)",
					u8"Nvidia CUDA (http://www.nvidia.com)",
				}
			};
			
			static const vector<string> music_credits {
				{
					u8"<b>Background Music</b>",
					u8"Project Divinity / Divinity (Jamendo / CC BY-NC-SA 3.0)",
					u8"Silence / Particule (Jamendo / LAL 1.3)",
					u8"Kachkin / Sound Crystals (Jamendo / CC BY-NC-ND 3.0)",
					u8"",
					u8"<b>Game Sounds</b>",
					u8"soundbible.com (Attribution / CC BY 3.0)",
					u8"freesfx.co.uk (http://www.freesfx.co.uk/info/eula/)",
					u8"soundjay.com (http://www.soundjay.com/tos.html royalty-free)",
					u8"GarageBand (http://www.apple.com royalty-free)",
					u8"sounddogs.com (http://sounddogs.com/htm/license.htm)",
					u8"wikipedia.org (Public Domain)",
					u8"",
				}
			};
			
			//
			static constexpr float credits_element_height = 0.03f;
			static constexpr float credits_row_offset = credits_element_height * 0.1f;
			
			float x_offset = 0.05f;
			const auto add_credit_line = [&x_offset,&next_height,this](const string& credit_line) {
				gui_text* line = ui->add<gui_text>(float2(0.45f, credits_element_height),
												   float2(x_offset, next_height(credits_element_height,
																				credits_row_offset)));
				line->set_label(credit_line);
				line->set_shade(true);
				menu_wnd->add_child(line);
				credits_ui.lines.push_back(line);
			};
			
			height = 0.05f;
			x_offset = 0.05f;
			for(const auto& credit_line : bim_credits) {
				add_credit_line(credit_line);
			}
			
			for(const auto& credit_line : lib_credits) {
				add_credit_line(credit_line);
			}
			
			height = 0.05f;
			x_offset = 0.5f;
			for(const auto& credit_line : music_credits) {
				add_credit_line(credit_line);
			}
		}
		break;
		case MENU_TYPE::__MAX_MENU_TYPE:
			// empty
			break;
	}
}

void menu_ui::save_config() const {
	e->get_xml()->save_file(e->get_config_doc(), e->data_path("config.xml"),
							"<!DOCTYPE config PUBLIC \"-//A2E//DTD config 1.0//EN\" \"config.dtd\">");
}

void menu_ui::update_controls() {
	if(active_menu != MENU_TYPE::CONTROLS) return;
	
	e->acquire_gl_context();
	unsigned int action_enum = (unsigned int)controls::ACTION::NONE;
	for(auto& action : controls_ui.actions) {
		action_enum++;
		const auto action_events = controls::get((controls::ACTION)action_enum);
		action.mapping[0]->set_input(controls::name(action_events.first));
		action.mapping[1]->set_input(controls::name(action_events.second));
	}
	controls::save();
	save_config();
	e->release_gl_context();
}
