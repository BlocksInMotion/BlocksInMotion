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

#ifndef __SB_MENU_UI_H__
#define __SB_MENU_UI_H__

#include "sb_global.h"
#include <gui/gui.h>

class font;
class gui_button;
class gui_text;
class gui_pop_up_button;
class gui_input;
class gui_toggle_button;
class gui_slider;
class gui_list_box;
class gui_input_box;
class audio_background;
class menu_ui {
public:
	menu_ui();
	~menu_ui();
	
	void set_enabled(const bool state);
	bool is_enabled() const;
	void back_to_main();
	
	void handle();
	
protected:
	bool enabled = false; // don't change this here
	bool editor_was_active = false;
	bool game_was_active = false;
	font* fnt = nullptr;
	audio_background* menu_music = nullptr;
	
	//
	enum class MENU_TYPE : unsigned int {
		MAIN,
		NEW_GAME,
		LOAD,
		HIGHSCORE,
		OPTIONS,
		CONTROLS,
		EDITOR,
		NEW_MAP,
		CREDITS,
		// no need for QUIT
		__MAX_MENU_TYPE
	};
	MENU_TYPE active_menu { MENU_TYPE::__MAX_MENU_TYPE };
	atomic<unsigned int> next_menu { (unsigned int)active_menu };
	gui_window* menu_wnd = nullptr;
	
	void open_menu(const MENU_TYPE type);
	
	//
	struct {
		gui_button* new_game_button = nullptr;
		gui_button* load_button = nullptr;
		gui_button* save_button = nullptr;
		gui_button* highscore_button = nullptr;
		gui_button* options_button = nullptr;
		gui_button* controls_button = nullptr;
		gui_button* editor_button = nullptr;
		gui_button* credits_button = nullptr;
		gui_button* quit_button = nullptr;
	} main_ui;
	struct {
		gui_text* enter_name_text = nullptr;
		gui_input_box* name_input = nullptr;
		gui_button* start_button = nullptr;
	} new_game_ui;
	struct {
		gui_list_box* save_list = nullptr;
		gui_button* load_button = nullptr;
	} load_ui;
	struct {
		gui_list_box* highscore_list = nullptr;
	} highscore_ui;
	struct {
		gui_text* screen_res_text = nullptr;
		gui_text* filtering_text = nullptr;
		gui_text* aniso_filtering_text = nullptr;
		gui_text* anti_aliasing_text = nullptr;
		gui_text* tex_quality_text = nullptr;
		gui_text* music_volume_text = nullptr;
		gui_text* sound_volume_text = nullptr;
		gui_text* fov_text = nullptr;
		gui_text* upscaling_text = nullptr;
		gui_text* cl_platform_text = nullptr;
		gui_text* cl_restrict_text = nullptr;
		
		//
		gui_pop_up_button* screen_res_popup = nullptr;
		gui_toggle_button* fullscreen_toggle = nullptr;
		gui_toggle_button* vsync_toggle = nullptr;
		gui_pop_up_button* filtering_popup = nullptr;
		gui_pop_up_button* filtering_aniso_popup = nullptr;
		gui_pop_up_button* anti_aliasing_popup = nullptr;
		gui_pop_up_button* tex_quality_popup = nullptr;
		gui_slider* music_volume_slider = nullptr;
		gui_slider* sound_volume_slider = nullptr;
		gui_slider* fov_slider = nullptr;
		gui_text* music_volume_value = nullptr;
		gui_text* sound_volume_value = nullptr;
		gui_text* fov_value = nullptr;
		gui_pop_up_button* upscaling_popup = nullptr;
		gui_pop_up_button* cl_platform_popup = nullptr;
		gui_pop_up_button* cl_restrict_popup = nullptr;
	} options_ui;
	struct {
		struct {
			gui_text* desc;
			gui_input_box* mapping[2];
		} actions[((unsigned int)controls::ACTION::__MAX_ACTION)-1];
		gui_input_box* active_input = nullptr;
		controls::ACTION active_action = controls::ACTION::NONE;
		bool active_side = true; // primary: true, secondary: false
	} controls_ui;
	struct {
		gui_list_box* map_list = nullptr;
		gui_button* edit_button = nullptr;
		gui_button* new_button = nullptr;
	} edit_ui;
	struct {
		gui_text* enter_map_name_text = nullptr;
		gui_input_box* map_name_input = nullptr;
		gui_button* create_button = nullptr;
	} new_map_ui;
	struct {
		// credits
		vector<gui_text*> lines;
	} credits_ui;
	void save_config() const;
	void update_controls();
	
	//
	event::handler evt_handler_fnctr;
	bool event_handler(EVENT_TYPE type, shared_ptr<event_object> obj);
	
};

#endif
