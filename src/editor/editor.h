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

#ifndef __SB_EDITOR_H__
#define __SB_EDITOR_H__

#include "sb_global.h"
#include "sb_map.h"
#include "../game/game_base.h"
#include <gui/gui.h>
#include <scene/scene.h>

class gui_button;
class gui_toggle_button;
class gui_pop_up_button;
class gui_input_box;
class gui_text;
class gui_slider;
enum class BLOCK_FACE : unsigned int;
class editor : public game_base {
public:
	editor();
	virtual ~editor();
	
	virtual void set_enabled(const bool state);
	
	void run();
	
	enum class EDITOR_MODE {
		BLOCK_MODE,
		OBJECT_MODE,
	};
	void set_mode(const EDITOR_MODE mode);
	const EDITOR_MODE& get_mode() const;
	
	void open_ui(); // main ui
	void close_ui();
	
protected:
	gui_theme* ui_theme;
	
	GLuint cube_vbo;
	GLuint cube_indices_vbo;
	
	EDITOR_MODE editor_mode = EDITOR_MODE::BLOCK_MODE;
	
	ui_draw_callback draw_cb;
	gui_simple_callback* ui_cb_obj = nullptr;
	void draw_ui(const DRAW_MODE_UI draw_mode, rtt::fbo* buffer);
	
	event::handler event_handler_fnctr;
	bool event_handler(EVENT_TYPE type, shared_ptr<event_object> obj);
	
	//
	scene::draw_callback scene_draw_cb;
	void draw_selection(const DRAW_MODE draw_mode);
	
	//
	enum class SELECTION_MODE {
		NONE,
		REMOVE,
		CREATE
	};
	void start_selection(const SELECTION_MODE mode);
	void end_selection(const SELECTION_MODE mode);
	void update_selection();
	void modify_selection(const SELECTION_MODE mode);
	SELECTION_MODE cur_selection_mode = SELECTION_MODE::NONE;
	pair<uint3, BLOCK_FACE> selected_block;
	// <start, end>
	pair<decltype(selected_block), decltype(selected_block)> block_selection;
	
	size_t selected_item = 0;
	size_t selected_item_index = 0;
	vector<pair<size_t, EDITOR_MODE>> selection_menu_items;
	
	// object mode:
	enum class OBJECT_TYPE : size_t {
		SOUND,
		MAP_LINK,
		TRIGGER,
		LIGHT_COLOR_AREA,
		AI_WAYPOINT,
		__MAX_OBJECT_TYPE
	};
	struct {
		OBJECT_TYPE type = OBJECT_TYPE::__MAX_OBJECT_TYPE;
		audio_3d* sound = nullptr;
		sb_map::map_link* link = nullptr;
		sb_map::trigger* trgr = nullptr;
		sb_map::light_color_area* lca = nullptr;
		sb_map::ai_waypoint* wp = nullptr;
		void reset() {
			type = OBJECT_TYPE::__MAX_OBJECT_TYPE;
			sound = nullptr;
			link = nullptr;
			trgr = nullptr;
			lca = nullptr;
			wp = nullptr;
		}
		bool is_selected(void* ptr) {
			// hacky, but good enough
			switch(type) {
				case OBJECT_TYPE::SOUND: return (sound == ptr);
				case OBJECT_TYPE::MAP_LINK: return (link == ptr);
				case OBJECT_TYPE::TRIGGER: return (trgr == ptr);
				case OBJECT_TYPE::LIGHT_COLOR_AREA: return (lca == ptr);
				case OBJECT_TYPE::AI_WAYPOINT: return (wp == ptr);
				case OBJECT_TYPE::__MAX_OBJECT_TYPE: return false;
			}
			a2e_unreachable();
		}
		void* get_ptr() {
			switch(type) {
				case OBJECT_TYPE::SOUND: return sound;
				case OBJECT_TYPE::MAP_LINK: return link;
				case OBJECT_TYPE::TRIGGER: return trgr;
				case OBJECT_TYPE::LIGHT_COLOR_AREA: return lca;
				case OBJECT_TYPE::AI_WAYPOINT: return wp;
				case OBJECT_TYPE::__MAX_OBJECT_TYPE: return nullptr;
			}
			a2e_unreachable();
		}
	} object_selection;
	static constexpr size_t object_count = (size_t)OBJECT_TYPE::__MAX_OBJECT_TYPE;
	array<a2e_texture, object_count+1> object_textures;
	GLuint object_texture_array = 0;
	GLuint object_sprites_ubo = 0;
	GLuint object_sprite_vbo = 0;
	unsigned int active_sprites = 0;
	array<float4, 65536/16> object_sprites_ubo_data;
	OBJECT_TYPE intersect_objects(ipnt wnd_pos);
	void add_map_object(const uint3& block_pos);
	
	light* editor_light = nullptr;
	
	//
	void switch_input_mode(const bool state); // false: 2d, true: 3d
	
	void open_object_ui(const OBJECT_TYPE& type, void* ptr);
	void update_object_ui();
	
	void open_settings_ui();
	void update_settings_ui();
	
	void close_workspace_ui();
	
	gui_window* main_wnd = nullptr;
	struct main_ui_data {
		gui_button* settings_button = nullptr;
		gui_button* cam_button = nullptr;
		gui_toggle_button* culling_button = nullptr;
		gui_toggle_button* light_button = nullptr;
		gui_button* save_button = nullptr;
		gui_button* exit_button = nullptr;
		void set_enabled(const bool& state);
	} main_ui;

	gui_window* workspace_wnd = nullptr;
	struct {
		gui_text* identifier_text = nullptr;
		gui_input_box* identifier_input = nullptr;
		gui_text* file_text = nullptr;
		gui_pop_up_button* file_button = nullptr;
		gui_text* position_text = nullptr;
		array<gui_button*, 6> position_buttons {
			{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, }
		};
		gui_text* play_on_load_text = nullptr;
		gui_toggle_button* play_on_load_button = nullptr;
		gui_text* loop_text = nullptr;
		gui_toggle_button* loop_button = nullptr;
		gui_text* volume_text = nullptr;
		gui_input_box* volume_input = nullptr;
		gui_text* ref_dist_text = nullptr;
		gui_input_box* ref_dist_input = nullptr;
		gui_text* rolloff_text = nullptr;
		gui_input_box* rolloff_input = nullptr;
		gui_text* max_dist_text = nullptr;
		gui_input_box* max_dist_input = nullptr;
		gui_button* play_button = nullptr;
		gui_button* stop_button = nullptr;
	} sound_ui;
	struct {
		gui_text* dst_text = nullptr;
		gui_pop_up_button* dst_button = nullptr;
		gui_text* identifier_text = nullptr;
		gui_input_box* identifier_input = nullptr;
		gui_text* enabled_text = nullptr;
		gui_toggle_button* enabled_button = nullptr;
		gui_text* position_text = nullptr;
		array<gui_button*, 6> position_buttons {
			{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, }
		};
		gui_text* size_text = nullptr;
		array<gui_button*, 6> size_buttons {
			{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, }
		};
	} map_link_ui;
	struct {
		gui_text* type_text = nullptr;
		gui_pop_up_button* type_button = nullptr;
		gui_text* sub_type_text = nullptr;
		gui_pop_up_button* sub_type_button = nullptr;
		gui_text* identifier_text = nullptr;
		gui_input_box* identifier_input = nullptr;
		gui_text* script_text = nullptr;
		gui_pop_up_button* script_button = nullptr;
		gui_text* on_load_text = nullptr;
		gui_pop_up_button* on_load_button = nullptr;
		gui_text* on_trigger_text = nullptr;
		gui_pop_up_button* on_trigger_button = nullptr;
		gui_text* on_untrigger_text = nullptr;
		gui_pop_up_button* on_untrigger_button = nullptr;
		gui_text* position_text = nullptr;
		array<gui_button*, 6> position_buttons {
			{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, }
		};
		gui_text* facing_text = nullptr;
		array<gui_toggle_button*, 6> facing_buttons {
			{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, }
		};
		
		// type dependent:
		gui_text* weight_text = nullptr;
		gui_input_box* weight_input = nullptr;
		gui_text* intensity_text = nullptr;
		gui_input_box* intensity_input = nullptr;
		gui_text* timer_text = nullptr;
		gui_input_box* timer_input = nullptr;
	} trigger_ui;
	struct {
		gui_text* position_text = nullptr;
		array<gui_button*, 6> position_buttons {
			{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, }
		};
		gui_text* size_text = nullptr;
		array<gui_button*, 6> size_buttons {
			{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, }
		};
		gui_text* light_color_text = nullptr;
		array<gui_slider*, 3> light_color_sliders {
			{ nullptr, nullptr, nullptr }
		};
	} light_color_area_ui;
	struct {
		gui_text* identifier_text = nullptr;
		gui_input_box* identifier_input = nullptr;
		gui_text* next_text = nullptr;
		gui_pop_up_button* next_button = nullptr;
		gui_text* position_text = nullptr;
		array<gui_button*, 6> position_buttons {
			{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, }
		};
	} ai_waypoint_ui;
	struct {
		gui_text* name_text = nullptr;
		gui_input_box* name_input = nullptr;
		gui_text* player_position_text = nullptr;
		array<gui_button*, 6> player_position_buttons {
			{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, }
		};
		gui_text* player_orientation_text = nullptr;
		gui_pop_up_button* player_orientation_button = nullptr;
		gui_text* background_audio_text = nullptr;
		gui_pop_up_button* background_audio_button = nullptr;
		gui_text* chunks_text = nullptr;
		array<gui_input_box*, 3> chunks_input {
			{ nullptr, nullptr, nullptr }
		};
		array<gui_button*, 6> chunks_button {
			{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, }
		};
		gui_text* light_color_text = nullptr;
		array<gui_slider*, 3> light_color_sliders {
			{ nullptr, nullptr, nullptr }
		};
	} map_settings_ui;
	
};

#endif
