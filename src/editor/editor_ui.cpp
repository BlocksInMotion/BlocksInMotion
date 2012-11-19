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
#include "audio_source.h"
#include "audio_3d.h"
#include "audio_background.h"
#include "audio_controller.h"
#include "physics_controller.h"
#include "rigid_body.h"
#include "soft_body.h"
#include "script.h"
#include "script_handler.h"
#include "map_renderer.h"
#include "map_storage.h"
#include <engine.h>
#include <rendering/gfx2d.h>
#include <scene/camera.h>
#include <gui/style/gui_surface.h>
#include <gui/style/gui_theme.h>
#include <gui/objects/gui_window.h>
#include <gui/objects/gui_text.h>
#include <gui/objects/gui_button.h>
#include <gui/objects/gui_input_box.h>
#include <gui/objects/gui_toggle_button.h>
#include <gui/objects/gui_pop_up_button.h>
#include <gui/objects/gui_slider.h>

// helper functions/variables
static constexpr float element_height = 0.06f;
static constexpr float row_offset = element_height * 0.125f;
static const array<float, 3> column_offsets { { 0.025f, 0.3f, 0.3f + element_height * 6.0f } };
static const array<float, 3> cell_sizes { {
	column_offsets[1] - column_offsets[0],
	column_offsets[2] - column_offsets[1],
	1.0f - column_offsets[2],
} };
static const float2 text_size(cell_sizes[0], element_height);
static const float2 data_size(cell_sizes[1], element_height);

static const array<string, 6> xyz_button_labels {
	// BLOCK_FACE order
	{ "+X", "-X", "+Y", "-Y", "+Z", "-Z" }
};

static void create_xyz_buttons(float& height,
							   gui_window* wnd,
							   array<gui_button*, 6>& buttons,
							   gui_object::handler&& handler) {
	for(size_t i = 0; i < 6; i++) {
		buttons[i] = ui->add<gui_button>(float2(element_height),
										 float2(column_offsets[1] + float(i)*element_height, height));
		buttons[i]->set_label(xyz_button_labels[i]);
		buttons[i]->add_handler(std::move(handler), GUI_EVENT::BUTTON_PRESS);
		wnd->add_child(buttons[i]);
	}
};

static void create_xyz_toggle_buttons(float& height,
									  gui_window* wnd,
									  array<gui_toggle_button*, 6>& buttons,
									  gui_object::handler&& handler) {
	for(size_t i = 0; i < 6; i++) {
		buttons[i] = ui->add<gui_toggle_button>(float2(element_height),
												float2(column_offsets[1] + float(i)*element_height, height));
		buttons[i]->set_label(xyz_button_labels[i], xyz_button_labels[i]);
		buttons[i]->add_handler(std::move(handler),
								GUI_EVENT::TOGGLE_BUTTON_ACTIVATION,
								GUI_EVENT::TOGGLE_BUTTON_DEACTIVATION);
		wnd->add_child(buttons[i]);
	}
};

static void fill_pop_up_button(gui_pop_up_button* button, vector<string> items) {
	for(const auto& item : items) {
		button->add_item(item, item);
	}
}

void editor::open_ui() {
	main_wnd = ui->add<gui_window>(float2(0.1f, 0.5f), float2(0.0f));
	workspace_wnd = ui->add<gui_window>(float2(0.5f), float2(0.0f, 0.5f));
	
	const float2 button_size(0.8f, element_height);
	float height = 0.0f;
	const auto next_height = [&height]() -> float {
		static const float row_offset = element_height * 0.0625f;
		const float ret { height };
		height += element_height + row_offset;
		return ret;
	};
	main_ui.settings_button = ui->add<gui_button>(button_size, float2(0.0f, next_height()));
	main_ui.cam_button = ui->add<gui_button>(button_size, float2(0.0f, next_height()));
	main_ui.culling_button = ui->add<gui_toggle_button>(button_size, float2(0.0f, next_height()));
	main_ui.light_button = ui->add<gui_toggle_button>(button_size, float2(0.0f, next_height()));
	main_ui.save_button = ui->add<gui_button>(button_size, float2(0.0f, next_height()));
	main_ui.exit_button = ui->add<gui_button>(button_size, float2(0.0f, next_height()));
	main_ui.settings_button->set_label("Map Settings");
	main_ui.cam_button->set_label("Camera");
	main_ui.culling_button->set_label("Culling", "No Culling");
	main_ui.light_button->set_label("Ambient", "No Ambient");
	main_ui.save_button->set_label("Save");
	main_ui.exit_button->set_label("Exit Editor");
	main_wnd->add_child(main_ui.settings_button);
	main_wnd->add_child(main_ui.cam_button);
	main_wnd->add_child(main_ui.culling_button);
	main_wnd->add_child(main_ui.light_button);
	main_wnd->add_child(main_ui.save_button);
	main_wnd->add_child(main_ui.exit_button);
	
	//
	main_ui.settings_button->add_handler([this](GUI_EVENT gevt a2e_unused, gui_object& obj a2e_unused) {
		e->acquire_gl_context();
		close_workspace_ui();
		object_selection.reset();
		open_settings_ui();
		e->release_gl_context();
	}, GUI_EVENT::BUTTON_PRESS);
	
	main_ui.cam_button->add_handler([this](GUI_EVENT gevt a2e_unused, gui_object& obj a2e_unused) {
		e->acquire_gl_context();
		close_workspace_ui();
		object_selection.reset();
		switch_input_mode(true);
		e->release_gl_context();
	}, GUI_EVENT::BUTTON_PRESS);
	
	mr->set_culling(false);
	main_ui.culling_button->set_toggled(false);
	main_ui.culling_button->add_handler([this](GUI_EVENT gevt a2e_unused, gui_object& obj a2e_unused) {
		e->acquire_gl_context();
		mr->set_culling(main_ui.culling_button->is_toggled());
		e->release_gl_context();
	}, GUI_EVENT::TOGGLE_BUTTON_ACTIVATION, GUI_EVENT::TOGGLE_BUTTON_DEACTIVATION);
	
	sce->delete_light(editor_light);
	main_ui.light_button->set_toggled(false);
	main_ui.light_button->add_handler([this](GUI_EVENT gevt a2e_unused, gui_object& obj a2e_unused) {
		e->acquire_gl_context();
		if(main_ui.light_button->is_toggled()) {
			sce->add_light(editor_light);
		}
		else sce->delete_light(editor_light);
		e->release_gl_context();
	}, GUI_EVENT::TOGGLE_BUTTON_ACTIVATION, GUI_EVENT::TOGGLE_BUTTON_DEACTIVATION);
	
	main_ui.save_button->add_handler([this](GUI_EVENT gevt a2e_unused, gui_object& obj a2e_unused) {
		e->acquire_gl_context();
		//
		const string& filename = active_map->get_filename();
		a2e_debug("saving map as \"%s\" ...", filename);
		if(!map_storage::save(filename, *active_map)) {
			a2e_error("failed to save map!");
		}
		else {
			a2e_debug("map successfully saved!");
		}
		e->release_gl_context();
	}, GUI_EVENT::BUTTON_PRESS);
	
	main_ui.exit_button->add_handler([this](GUI_EVENT gevt a2e_unused, gui_object& obj a2e_unused) {
		e->acquire_gl_context();
		set_enabled(false);
		e->release_gl_context();
	}, GUI_EVENT::BUTTON_PRESS);
}

void editor::switch_input_mode(const bool state) {
	main_ui.set_enabled(state ^ true);
	cam->set_keyboard_input(state);
	cam->set_mouse_input(state);
	cam->set_wasd_input(state);
}

void editor::main_ui_data::set_enabled(const bool& state) {
	settings_button->set_enabled(state);
	cam_button->set_enabled(state);
	culling_button->set_enabled(state);
	light_button->set_enabled(state);
	save_button->set_enabled(state);
	exit_button->set_enabled(state);
}

void editor::close_ui() {
	object_selection.reset();
	close_workspace_ui();
	
	if(main_wnd != nullptr) {
		ui->remove<gui_window>(main_wnd);
		delete main_wnd;
		main_wnd = nullptr;
	}
	
	if(workspace_wnd != nullptr) {
		ui->remove<gui_window>(workspace_wnd);
		delete workspace_wnd;
		workspace_wnd = nullptr;
	}
}

void editor::open_object_ui(const OBJECT_TYPE& type, void* ptr a2e_unused) {
	if(workspace_wnd == nullptr) return;
	close_workspace_ui(); // close old ui
	
	float height = 0.0f;
	const auto next_height = [&height]() -> float {
		const float ret { height };
		height += element_height + row_offset;
		return ret;
	};
	const auto reset_height = [&height]() { height = row_offset; };
	reset_height();
	
	// create ui objects
	switch(type) {
		case OBJECT_TYPE::SOUND: {
			//// text
			sound_ui.identifier_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			sound_ui.file_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			sound_ui.position_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			sound_ui.play_on_load_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			sound_ui.loop_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			sound_ui.volume_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			sound_ui.ref_dist_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			sound_ui.rolloff_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			sound_ui.max_dist_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			sound_ui.identifier_text->set_label("Identifier");
			sound_ui.file_text->set_label("File");
			sound_ui.position_text->set_label("Position");
			sound_ui.play_on_load_text->set_label("Play on load");
			sound_ui.loop_text->set_label("Loop");
			sound_ui.volume_text->set_label("Volume");
			sound_ui.ref_dist_text->set_label("Reference Distance");
			sound_ui.rolloff_text->set_label("Rolloff Factor");
			sound_ui.max_dist_text->set_label("Max Distance");
			sound_ui.identifier_text->set_shade(true);
			sound_ui.file_text->set_shade(true);
			sound_ui.position_text->set_shade(true);
			sound_ui.play_on_load_text->set_shade(true);
			sound_ui.loop_text->set_shade(true);
			sound_ui.volume_text->set_shade(true);
			sound_ui.ref_dist_text->set_shade(true);
			sound_ui.rolloff_text->set_shade(true);
			sound_ui.max_dist_text->set_shade(true);
			workspace_wnd->add_child(sound_ui.identifier_text);
			workspace_wnd->add_child(sound_ui.file_text);
			workspace_wnd->add_child(sound_ui.position_text);
			workspace_wnd->add_child(sound_ui.play_on_load_text);
			workspace_wnd->add_child(sound_ui.loop_text);
			workspace_wnd->add_child(sound_ui.volume_text);
			workspace_wnd->add_child(sound_ui.ref_dist_text);
			workspace_wnd->add_child(sound_ui.rolloff_text);
			workspace_wnd->add_child(sound_ui.max_dist_text);
			
			////
			reset_height();
			sound_ui.identifier_input = ui->add<gui_input_box>(data_size, float2(column_offsets[1], next_height()));
			sound_ui.file_button = ui->add<gui_pop_up_button>(data_size, float2(column_offsets[1], next_height()));
			
			const auto& store(as->get_store());
			set<string> audio_identifiers;
			for(const auto& entry : store) {
				if(entry.first == "BACKGROUND_MUSIC") continue;
				audio_identifiers.insert(entry.first);
			}
			for(const auto& identifier : audio_identifiers) {
				sound_ui.file_button->add_item(identifier, identifier);
			}
			
			create_xyz_buttons(height, workspace_wnd,
							   sound_ui.position_buttons, [this](GUI_EVENT gevt a2e_unused, gui_object& obj) {
				e->acquire_gl_context();
				int3 dir;
				for(size_t i = 0; i < 6; i++) {
					if(sound_ui.position_buttons[i] == &obj) {
						dir[i/2] = ((i % 2) == 0) ? 1 : -1;
						break;
					}
				}
				if(dir.is_null()) {
					e->release_gl_context();
					return;
				}
				
				audio_3d* snd = (audio_3d*)object_selection.get_ptr();
				const uint3 new_pos(int3(snd->get_position().floored()) + dir);
				if(active_map->is_valid_position(new_pos)) {
					snd->set_position(float3(new_pos) + 0.5f);
				}
				
				e->release_gl_context();
			});
			next_height();
			
			sound_ui.play_on_load_button = ui->add<gui_toggle_button>(data_size, float2(column_offsets[1], next_height()));
			sound_ui.loop_button = ui->add<gui_toggle_button>(data_size, float2(column_offsets[1], next_height()));
			sound_ui.volume_input = ui->add<gui_input_box>(data_size, float2(column_offsets[1], next_height()));
			sound_ui.ref_dist_input = ui->add<gui_input_box>(data_size, float2(column_offsets[1], next_height()));
			sound_ui.rolloff_input = ui->add<gui_input_box>(data_size, float2(column_offsets[1], next_height()));
			sound_ui.max_dist_input = ui->add<gui_input_box>(data_size, float2(column_offsets[1], next_height()));
			sound_ui.play_button = ui->add<gui_button>(float2(data_size.x * 0.5f, data_size.y),
													   float2(column_offsets[1], height));
			sound_ui.stop_button = ui->add<gui_button>(float2(data_size.x * 0.5f, data_size.y),
													   float2(column_offsets[1] + (data_size.x * 0.5f), height));
			next_height();
			
			workspace_wnd->add_child(sound_ui.identifier_input);
			workspace_wnd->add_child(sound_ui.file_button);
			workspace_wnd->add_child(sound_ui.play_on_load_button);
			workspace_wnd->add_child(sound_ui.loop_button);
			workspace_wnd->add_child(sound_ui.volume_input);
			workspace_wnd->add_child(sound_ui.ref_dist_input);
			workspace_wnd->add_child(sound_ui.rolloff_input);
			workspace_wnd->add_child(sound_ui.max_dist_input);
			workspace_wnd->add_child(sound_ui.play_button);
			workspace_wnd->add_child(sound_ui.stop_button);
			
			sound_ui.play_on_load_button->set_label("yes", "no");
			sound_ui.loop_button->set_label("yes", "no");
			sound_ui.play_button->set_label("play");
			sound_ui.stop_button->set_label("stop");
			
			//
			sound_ui.identifier_input->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				audio_3d* snd = (audio_3d*)object_selection.get_ptr();
				const string& identifier(sound_ui.identifier_input->get_input());
				const string& snd_identifier(snd->get_identifier());
				const size_t point_pos(snd_identifier.find("."));
				const string& combined_identifier(snd_identifier.substr(0, point_pos) + "." + identifier);
				
				if(combined_identifier == snd->get_identifier()) return;
				
				if(ac->get_audio_source(combined_identifier) != nullptr) {
					a2e_error("an audio source with the identifier \"%s\" already exists!", identifier);
					sound_ui.identifier_input->set_input(snd_identifier.substr(point_pos+1, snd_identifier.size()-point_pos-1));
					e->release_gl_context();
					return;
				}
				
				if(!snd->set_identifier(combined_identifier)) {
					a2e_error("failed to set audio source identifier from \"%s\" to \"%s\"!",
							  snd_identifier, combined_identifier);
					e->release_gl_context();
					return;
				}
				
				ui->set_active_object(nullptr); // make input box inactive
				e->release_gl_context();
			}, GUI_EVENT::INPUT_BOX_ENTER);
			
			sound_ui.file_button->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				audio_3d* snd = (audio_3d*)object_selection.get_ptr();
				const string& snd_identifier(snd->get_identifier());
				const size_t point_pos(snd_identifier.find("."));
				const string type_identifier(snd_identifier.substr(0, point_pos));
				const string inst_identifier(snd_identifier.substr(point_pos+1, snd_identifier.size()-point_pos-1));
				const string& file_identifier(sound_ui.file_button->get_selected_item()->first);
				
				if(type_identifier == file_identifier) {
					e->release_gl_context();
					return;
				}
				
				const string combined_identifier(file_identifier + "." + inst_identifier);
				if(ac->get_audio_source(combined_identifier) != nullptr) {
					a2e_error("can't change the type of the audio source, because a source with the identifier \"%s\" already exists!", combined_identifier);
					sound_ui.file_button->set_selected_item(type_identifier);
					e->release_gl_context();
					return;
				}
				
				// create a new sound with the same setting (but different file)
				audio_3d* new_snd = ac->add_audio_3d(file_identifier, inst_identifier);
				new_snd->set_position(snd->get_position());
				new_snd->set_velocity(snd->get_velocity());
				new_snd->set_volume(snd->get_volume());
				new_snd->set_reference_distance(snd->get_reference_distance());
				new_snd->set_rolloff_factor(snd->get_rolloff_factor());
				new_snd->set_max_distance(snd->get_max_distance());
				if(snd->is_looping()) new_snd->loop();
				if(snd->is_playing()) new_snd->play();
				
				// delete old one
				active_map->remove_sound(snd);
				
				// add/set new one
				active_map->add_sound(new_snd);
				object_selection.sound = new_snd;
				
				update_object_ui();
				e->release_gl_context();
			}, GUI_EVENT::POP_UP_BUTTON_SELECT);
			
			sound_ui.play_on_load_button->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				audio_3d* snd = (audio_3d*)object_selection.get_ptr();
				const bool play_on_load(sound_ui.play_on_load_button->is_toggled());
				snd->set_play_on_load(play_on_load);
				if(play_on_load) snd->play();
				else snd->stop();
				e->release_gl_context();
			}, GUI_EVENT::TOGGLE_BUTTON_ACTIVATION, GUI_EVENT::TOGGLE_BUTTON_DEACTIVATION);
			
			sound_ui.loop_button->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				audio_3d* snd = (audio_3d*)object_selection.get_ptr();
				snd->loop(sound_ui.loop_button->is_toggled());
				e->release_gl_context();
			}, GUI_EVENT::TOGGLE_BUTTON_ACTIVATION, GUI_EVENT::TOGGLE_BUTTON_DEACTIVATION);
			
			sound_ui.volume_input->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				audio_3d* snd = (audio_3d*)object_selection.get_ptr();
				snd->set_volume(string2float(sound_ui.volume_input->get_input()));
				ui->set_active_object(nullptr); // make input box inactive
				e->release_gl_context();
			}, GUI_EVENT::INPUT_BOX_ENTER);
			
			sound_ui.ref_dist_input->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				audio_3d* snd = (audio_3d*)object_selection.get_ptr();
				snd->set_reference_distance(string2float(sound_ui.ref_dist_input->get_input()));
				ui->set_active_object(nullptr); // make input box inactive
				e->release_gl_context();
			}, GUI_EVENT::INPUT_BOX_ENTER);
			
			sound_ui.rolloff_input->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				audio_3d* snd = (audio_3d*)object_selection.get_ptr();
				snd->set_rolloff_factor(string2float(sound_ui.rolloff_input->get_input()));
				ui->set_active_object(nullptr); // make input box inactive
				e->release_gl_context();
			}, GUI_EVENT::INPUT_BOX_ENTER);
			
			sound_ui.max_dist_input->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				audio_3d* snd = (audio_3d*)object_selection.get_ptr();
				snd->set_max_distance(string2float(sound_ui.max_dist_input->get_input()));
				ui->set_active_object(nullptr); // make input box inactive
				e->release_gl_context();
			}, GUI_EVENT::INPUT_BOX_ENTER);
			
			sound_ui.play_button->add_handler([this](GUI_EVENT gevt a2e_unused, gui_object& obj a2e_unused) {
				e->acquire_gl_context();
				audio_3d* snd = (audio_3d*)object_selection.get_ptr();
				snd->play();
				e->release_gl_context();
			}, GUI_EVENT::BUTTON_PRESS);
			
			sound_ui.stop_button->add_handler([this](GUI_EVENT gevt a2e_unused, gui_object& obj a2e_unused) {
				e->acquire_gl_context();
				audio_3d* snd = (audio_3d*)object_selection.get_ptr();
				snd->stop();
				e->release_gl_context();
			}, GUI_EVENT::BUTTON_PRESS);
		}
		break;
		case OBJECT_TYPE::MAP_LINK: {
			//// text
			map_link_ui.dst_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			map_link_ui.identifier_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			map_link_ui.enabled_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			map_link_ui.position_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			map_link_ui.size_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			map_link_ui.dst_text->set_label("Destination");
			map_link_ui.identifier_text->set_label("Identifier");
			map_link_ui.enabled_text->set_label("Enabled");
			map_link_ui.position_text->set_label("Position");
			map_link_ui.size_text->set_label("Size");
			map_link_ui.dst_text->set_shade(true);
			map_link_ui.identifier_text->set_shade(true);
			map_link_ui.enabled_text->set_shade(true);
			map_link_ui.position_text->set_shade(true);
			map_link_ui.size_text->set_shade(true);
			workspace_wnd->add_child(map_link_ui.dst_text);
			workspace_wnd->add_child(map_link_ui.identifier_text);
			workspace_wnd->add_child(map_link_ui.enabled_text);
			workspace_wnd->add_child(map_link_ui.position_text);
			workspace_wnd->add_child(map_link_ui.size_text);
			
			////
			reset_height();
			map_link_ui.dst_button = ui->add<gui_pop_up_button>(data_size, float2(column_offsets[1], next_height()));
			map_link_ui.identifier_input = ui->add<gui_input_box>(data_size, float2(column_offsets[1], next_height()));
			map_link_ui.enabled_button = ui->add<gui_toggle_button>(data_size, float2(column_offsets[1], next_height()));
			workspace_wnd->add_child(map_link_ui.dst_button);
			workspace_wnd->add_child(map_link_ui.identifier_input);
			workspace_wnd->add_child(map_link_ui.enabled_button);
			
			create_xyz_buttons(height, workspace_wnd,
							   map_link_ui.position_buttons, [this](GUI_EVENT, gui_object& obj) {
				// since we're changing rendering data, acquire the engine lock
				e->acquire_gl_context();
				int3 dir;
				for(size_t i = 0; i < 6; i++) {
					if(map_link_ui.position_buttons[i] == &obj) {
						dir[i/2] = ((i % 2) == 0) ? 1 : -1;
						break;
					}
				}
				if(dir.is_null()) {
					e->release_gl_context();
					return;
				}
				
				sb_map::map_link* ml = (sb_map::map_link*)object_selection.get_ptr();
				for(auto pos_iter = begin(ml->positions); pos_iter != end(ml->positions);) {
					const uint3 moved_pos(int3(*pos_iter) + dir);
					if(active_map->is_valid_position(moved_pos)) {
						*pos_iter = moved_pos;
						pos_iter++;
					}
					else {
						pos_iter = ml->positions.erase(pos_iter);
					}
				}
				if(ml->positions.empty()) {
					object_selection.reset();
					active_map->remove_map_link(ml);
					close_workspace_ui();
				}
				e->release_gl_context();
			});
			next_height();
			create_xyz_buttons(height, workspace_wnd,
							   map_link_ui.size_buttons, [this](GUI_EVENT, gui_object& obj) {
				// since we're changing rendering data, acquire the engine lock
				e->acquire_gl_context();
				uint3 dir;
				uint2 layer;
				unsigned int dir_component = 0;
				bool decrease_size = true;
				for(unsigned int i = 0; i < 6; i++) {
					if(map_link_ui.size_buttons[i] == &obj) {
						decrease_size = (i % 2) == 1;
						dir_component = i/2;
						dir[dir_component] = 1;
						layer.set((dir_component+1) % 3, (dir_component+2) % 3);
						break;
					}
				}
				if(dir.is_null()) {
					e->release_gl_context();
					return;
				}
				
				// note: this will only change the size in the positive direction
				sb_map::map_link* ml = (sb_map::map_link*)object_selection.get_ptr();
				if(!ml->positions.empty()) {
					uint3 bbox_min(ml->positions[0]), bbox_max(ml->positions[0]);
					for(const auto& pos : ml->positions) {
						bbox_min.min(pos);
						bbox_max.max(pos);
					}
					
					// increase
					if(!decrease_size) {
						const uint3 inv_dir(uint3(1) - dir);
						const uint3 layer_start(bbox_min.scaled(inv_dir)), layer_end(bbox_max.scaled(inv_dir));
						const uint3 add_pos(bbox_max.scaled(dir) + dir);
						uint3 layer_pos;
						for(unsigned int i = layer_start[layer.x]; i <= layer_end[layer.x]; i++) {
							layer_pos[layer.x] = i;
							for(unsigned int j = layer_start[layer.y]; j <= layer_end[layer.y]; j++) {
								layer_pos[layer.y] = j;
								const uint3 block_pos(add_pos + layer_pos);
								if(active_map->is_valid_position(block_pos)) {
									ml->positions.emplace_back(block_pos);
								}
							}
						}
					}
					// decrease
					else {
						const unsigned int dec_value = (bbox_max * dir);
						for(auto pos_iter = begin(ml->positions); pos_iter != end(ml->positions);) {
							if((*pos_iter)[dir_component] == dec_value) {
								pos_iter = ml->positions.erase(pos_iter);
							}
							else pos_iter++;
						}
					}
				}
				if(ml->positions.empty()) {
					object_selection.reset();
					active_map->remove_map_link(ml);
					close_workspace_ui();
				}
				e->release_gl_context();
			});
			next_height();
			
			map_link_ui.enabled_button->set_label("enabled", "disabled");
			map_link_ui.enabled_button->add_handler([this](GUI_EVENT gevt, gui_object& obj a2e_unused) {
				sb_map::map_link* ml = (sb_map::map_link*)object_selection.get_ptr();
				ml->enabled = (gevt == GUI_EVENT::TOGGLE_BUTTON_ACTIVATION);
			}, GUI_EVENT::TOGGLE_BUTTON_ACTIVATION, GUI_EVENT::TOGGLE_BUTTON_DEACTIVATION);
			
			const auto map_list(core::get_file_list(e->data_path("maps/"), "map"));
			for(const auto& map_entry : map_list) {
				map_link_ui.dst_button->add_item(map_entry.first, map_entry.first);
			}
			map_link_ui.dst_button->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				sb_map::map_link* ml = (sb_map::map_link*)object_selection.get_ptr();
				ml->dst_map_name = map_link_ui.dst_button->get_selected_item()->first;
				e->release_gl_context();
			}, GUI_EVENT::POP_UP_BUTTON_SELECT);
			
			map_link_ui.identifier_input->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				sb_map::map_link* ml = (sb_map::map_link*)object_selection.get_ptr();
				ml->identifier = map_link_ui.identifier_input->get_input();
				ui->set_active_object(nullptr); // make input box inactive
				e->release_gl_context();
			}, GUI_EVENT::INPUT_BOX_ENTER);
		}
		break;
		case OBJECT_TYPE::TRIGGER: {
			//// text
			trigger_ui.type_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			trigger_ui.sub_type_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			trigger_ui.identifier_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			trigger_ui.script_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			trigger_ui.on_load_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			trigger_ui.on_trigger_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			trigger_ui.on_untrigger_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			trigger_ui.position_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			trigger_ui.facing_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			trigger_ui.type_text->set_label("Type");
			trigger_ui.sub_type_text->set_label("Sub-Type");
			trigger_ui.identifier_text->set_label("Identifier");
			trigger_ui.script_text->set_label("Script");
			trigger_ui.on_load_text->set_label("On-Load");
			trigger_ui.on_trigger_text->set_label("On-Trigger");
			trigger_ui.on_untrigger_text->set_label("On-Untrigger");
			trigger_ui.position_text->set_label("Position");
			trigger_ui.facing_text->set_label("Facing");
			trigger_ui.type_text->set_shade(true);
			trigger_ui.sub_type_text->set_shade(true);
			trigger_ui.identifier_text->set_shade(true);
			trigger_ui.script_text->set_shade(true);
			trigger_ui.on_load_text->set_shade(true);
			trigger_ui.on_trigger_text->set_shade(true);
			trigger_ui.on_untrigger_text->set_shade(true);
			trigger_ui.position_text->set_shade(true);
			trigger_ui.facing_text->set_shade(true);
			workspace_wnd->add_child(trigger_ui.type_text);
			workspace_wnd->add_child(trigger_ui.sub_type_text);
			workspace_wnd->add_child(trigger_ui.identifier_text);
			workspace_wnd->add_child(trigger_ui.script_text);
			workspace_wnd->add_child(trigger_ui.on_load_text);
			workspace_wnd->add_child(trigger_ui.on_trigger_text);
			workspace_wnd->add_child(trigger_ui.on_untrigger_text);
			workspace_wnd->add_child(trigger_ui.position_text);
			workspace_wnd->add_child(trigger_ui.facing_text);
			
			trigger_ui.weight_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			trigger_ui.intensity_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			trigger_ui.timer_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			trigger_ui.weight_text->set_label("Weight");
			trigger_ui.intensity_text->set_label("Intensity");
			trigger_ui.timer_text->set_label("Timer (ms)");
			trigger_ui.weight_text->set_shade(true);
			trigger_ui.intensity_text->set_shade(true);
			trigger_ui.timer_text->set_shade(true);
			workspace_wnd->add_child(trigger_ui.weight_text);
			workspace_wnd->add_child(trigger_ui.intensity_text);
			workspace_wnd->add_child(trigger_ui.timer_text);
			
			////
			reset_height();
			trigger_ui.type_button = ui->add<gui_pop_up_button>(data_size, float2(column_offsets[1], next_height()));
			trigger_ui.sub_type_button = ui->add<gui_pop_up_button>(data_size, float2(column_offsets[1], next_height()));
			trigger_ui.identifier_input = ui->add<gui_input_box>(data_size, float2(column_offsets[1], next_height()));
			trigger_ui.script_button = ui->add<gui_pop_up_button>(data_size, float2(column_offsets[1], next_height()));
			trigger_ui.on_load_button = ui->add<gui_pop_up_button>(data_size, float2(column_offsets[1], next_height()));
			trigger_ui.on_trigger_button = ui->add<gui_pop_up_button>(data_size, float2(column_offsets[1], next_height()));
			trigger_ui.on_untrigger_button = ui->add<gui_pop_up_button>(data_size, float2(column_offsets[1], next_height()));
			workspace_wnd->add_child(trigger_ui.type_button);
			workspace_wnd->add_child(trigger_ui.sub_type_button);
			workspace_wnd->add_child(trigger_ui.identifier_input);
			workspace_wnd->add_child(trigger_ui.script_button);
			workspace_wnd->add_child(trigger_ui.on_load_button);
			workspace_wnd->add_child(trigger_ui.on_trigger_button);
			workspace_wnd->add_child(trigger_ui.on_untrigger_button);
			
			create_xyz_buttons(height, workspace_wnd,
							   trigger_ui.position_buttons, [this](GUI_EVENT, gui_object& obj) {
				e->acquire_gl_context();
				int3 dir;
				for(size_t i = 0; i < 6; i++) {
					if(trigger_ui.position_buttons[i] == &obj) {
						dir[i/2] = ((i % 2) == 0) ? 1 : -1;
						break;
					}
				}
				if(dir.is_null()) {
					e->release_gl_context();
					return;
				}
				
				sb_map::trigger* trgr = (sb_map::trigger*)object_selection.get_ptr();
				const uint3 new_pos(int3(trgr->position) + dir);
				if(active_map->is_valid_position(new_pos)) {
					trgr->position = new_pos;
				}
				
				e->release_gl_context();
			});
			next_height();
			create_xyz_toggle_buttons(height, workspace_wnd,
									  trigger_ui.facing_buttons, [this](GUI_EVENT, gui_object& obj) {
				e->acquire_gl_context();
				sb_map::trigger* trgr = (sb_map::trigger*)object_selection.get_ptr();
				for(unsigned int i = 0; i < 6; i++) {
					if(trigger_ui.facing_buttons[i] == &obj) {
						if(trigger_ui.facing_buttons[i]->is_toggled()) {
							trgr->facing = (BLOCK_FACE)(((unsigned int)trgr->facing) | (1u << i));
						}
						else trgr->facing = (BLOCK_FACE)(((unsigned int)trgr->facing) & ~(1u << i));
					}
				}
				e->release_gl_context();
			});
			next_height();
			
			fill_pop_up_button(trigger_ui.type_button, { "None", "Push", "Weight", "Light" });
			fill_pop_up_button(trigger_ui.sub_type_button, { "None", "Timed" });
			const auto script_list(core::get_file_list(e->data_path("scripts/"), "script"));
			for(const auto& script_entry : script_list) {
				trigger_ui.script_button->add_item(script_entry.first, script_entry.first);
			}
			fill_pop_up_button(trigger_ui.on_load_button, { "None" });
			fill_pop_up_button(trigger_ui.on_trigger_button, { "None" });
			fill_pop_up_button(trigger_ui.on_untrigger_button, { "None" });
			
			trigger_ui.type_button->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				sb_map::trigger* trgr = (sb_map::trigger*)object_selection.get_ptr();
				const string& type_str(trigger_ui.type_button->get_selected_item()->first);
				TRIGGER_TYPE new_type = TRIGGER_TYPE::NONE;
				if(type_str == "None") new_type = TRIGGER_TYPE::NONE;
				else if(type_str == "Push") new_type = TRIGGER_TYPE::PUSH;
				else if(type_str == "Weight") new_type = TRIGGER_TYPE::WEIGHT;
				else if(type_str == "Light") new_type = TRIGGER_TYPE::LIGHT;
				
				if(trgr->type != new_type) {
					// to keep things sane, simply remove the trigger and add a new one
					sb_map::trigger* new_trgr = new sb_map::trigger {
						trgr->identifier,
						new_type,
						trgr->sub_type,
						trgr->position,
						trgr->facing,
						trgr->scr,
						trgr->on_load,
						trgr->on_trigger,
						trgr->on_untrigger,
						// dependent data:
						(trgr->weight <= EPSILON && new_type == TRIGGER_TYPE::WEIGHT ?
						 0.1f : 0.0f),
						trgr->intensity,
						trgr->time,
						sb_map::trigger::state_struct()
					};
					active_map->add_trigger(new_trgr);
					active_map->remove_trigger(trgr);
					object_selection.trgr = new_trgr;
					update_object_ui();
				}
				
				e->release_gl_context();
			}, GUI_EVENT::POP_UP_BUTTON_SELECT);
			
			trigger_ui.sub_type_button->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				sb_map::trigger* trgr = (sb_map::trigger*)object_selection.get_ptr();
				const string& sub_type_str(trigger_ui.sub_type_button->get_selected_item()->first);
				if(sub_type_str == "None" && trgr->sub_type != TRIGGER_SUB_TYPE::NONE) {
					trgr->sub_type = TRIGGER_SUB_TYPE::NONE;
				}
				else if(sub_type_str == "Timed" && trgr->sub_type != TRIGGER_SUB_TYPE::TIMED) {
					trgr->sub_type = TRIGGER_SUB_TYPE::TIMED;
				}
				else {
					// nothing to update
					e->release_gl_context();
					return;
				}
				update_object_ui();
				e->release_gl_context();
			}, GUI_EVENT::POP_UP_BUTTON_SELECT);
			
			trigger_ui.script_button->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				sb_map::trigger* trgr = (sb_map::trigger*)object_selection.get_ptr();
				const string& script_str(trigger_ui.script_button->get_selected_item()->first);
				trgr->scr = sh->reload_script(script_str);
				update_object_ui();
				e->release_gl_context();
			}, GUI_EVENT::POP_UP_BUTTON_SELECT);
			
			trigger_ui.on_load_button->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				sb_map::trigger* trgr = (sb_map::trigger*)object_selection.get_ptr();
				const string& item_str(trigger_ui.on_load_button->get_selected_item()->first);
				if(item_str == "None") {
					trgr->on_load = "";
				}
				else trgr->on_load = item_str;
				e->release_gl_context();
			}, GUI_EVENT::POP_UP_BUTTON_SELECT);
			
			trigger_ui.on_trigger_button->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				sb_map::trigger* trgr = (sb_map::trigger*)object_selection.get_ptr();
				const string& item_str(trigger_ui.on_trigger_button->get_selected_item()->first);
				if(item_str == "None") {
					trgr->on_trigger = "";
				}
				else trgr->on_trigger = item_str;
				e->release_gl_context();
			}, GUI_EVENT::POP_UP_BUTTON_SELECT);
			
			trigger_ui.on_untrigger_button->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				sb_map::trigger* trgr = (sb_map::trigger*)object_selection.get_ptr();
				const string& item_str(trigger_ui.on_untrigger_button->get_selected_item()->first);
				if(item_str == "None") {
					trgr->on_untrigger = "";
				}
				else trgr->on_untrigger = item_str;
				e->release_gl_context();
			}, GUI_EVENT::POP_UP_BUTTON_SELECT);
			
			trigger_ui.identifier_input->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				sb_map::trigger* trgr = (sb_map::trigger*)object_selection.get_ptr();
				trgr->identifier = trigger_ui.identifier_input->get_input();
				ui->set_active_object(nullptr); // make input box inactive
				e->release_gl_context();
			}, GUI_EVENT::INPUT_BOX_ENTER);
			
			//
			trigger_ui.weight_input = ui->add<gui_input_box>(data_size, float2(column_offsets[1], next_height()));
			trigger_ui.intensity_input = ui->add<gui_input_box>(data_size, float2(column_offsets[1], next_height()));
			trigger_ui.timer_input = ui->add<gui_input_box>(data_size, float2(column_offsets[1], next_height()));
			workspace_wnd->add_child(trigger_ui.weight_input);
			workspace_wnd->add_child(trigger_ui.intensity_input);
			workspace_wnd->add_child(trigger_ui.timer_input);
			
			trigger_ui.weight_input->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				sb_map::trigger* trgr = (sb_map::trigger*)object_selection.get_ptr();
				trgr->weight = string2float(trigger_ui.weight_input->get_input());
				ui->set_active_object(nullptr); // make input box inactive
				e->release_gl_context();
			}, GUI_EVENT::INPUT_BOX_ENTER);
			
			trigger_ui.intensity_input->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				sb_map::trigger* trgr = (sb_map::trigger*)object_selection.get_ptr();
				trgr->intensity = string2float(trigger_ui.intensity_input->get_input());
				ui->set_active_object(nullptr); // make input box inactive
				e->release_gl_context();
			}, GUI_EVENT::INPUT_BOX_ENTER);
			
			trigger_ui.timer_input->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				sb_map::trigger* trgr = (sb_map::trigger*)object_selection.get_ptr();
				trgr->time = string2uint(trigger_ui.timer_input->get_input());
				ui->set_active_object(nullptr); // make input box inactive
				e->release_gl_context();
			}, GUI_EVENT::INPUT_BOX_ENTER);
		}
		break;
		case OBJECT_TYPE::LIGHT_COLOR_AREA: {
			//// text
			light_color_area_ui.position_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			light_color_area_ui.size_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			light_color_area_ui.light_color_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			light_color_area_ui.position_text->set_label("Position");
			light_color_area_ui.size_text->set_label("Size");
			light_color_area_ui.light_color_text->set_label("Color");
			light_color_area_ui.position_text->set_shade(true);
			light_color_area_ui.size_text->set_shade(true);
			light_color_area_ui.light_color_text->set_shade(true);
			workspace_wnd->add_child(light_color_area_ui.position_text);
			workspace_wnd->add_child(light_color_area_ui.size_text);
			workspace_wnd->add_child(light_color_area_ui.light_color_text);
			
			////
			reset_height();
			create_xyz_buttons(height, workspace_wnd,
							   light_color_area_ui.position_buttons, [this](GUI_EVENT, gui_object& obj)
			{
				e->acquire_gl_context();
				int3 dir;
				for(size_t i = 0; i < 6; i++) {
					if(light_color_area_ui.position_buttons[i] == &obj) {
						dir[i/2] = ((i % 2) == 0) ? 1 : -1;
						break;
					}
				}
				if(dir.is_null()) {
					e->release_gl_context();
					return;
				}
				
				sb_map::light_color_area* lca = (sb_map::light_color_area*)object_selection.get_ptr();
				const uint3 new_min_pos(int3(lca->min_pos) + dir), new_max_pos(int3(lca->max_pos) + dir);
				if(active_map->is_valid_position(new_min_pos)) {
					lca->min_pos = new_min_pos;
				}
				
				const uint3 map_max_pos(sb_map::chunk_extent * active_map->get_chunk_count());
				lca->max_pos.x = new_max_pos.x < map_max_pos.x ? new_max_pos.x : map_max_pos.x;
				lca->max_pos.y = new_max_pos.y < map_max_pos.y ? new_max_pos.y : map_max_pos.y;
				lca->max_pos.z = new_max_pos.z < map_max_pos.z ? new_max_pos.z : map_max_pos.z;
				
				active_map->update_light_colors();
				e->release_gl_context();
			});
			next_height();
			
			create_xyz_buttons(height, workspace_wnd,
							   light_color_area_ui.size_buttons, [this](GUI_EVENT, gui_object& obj)
			{
				e->acquire_gl_context();
				int3 dir;
				for(size_t i = 0; i < 6; i++) {
					if(light_color_area_ui.size_buttons[i] == &obj) {
						dir[i/2] = ((i % 2) == 0) ? 1 : -1;
						break;
					}
				}
				if(dir.is_null()) {
					e->release_gl_context();
					return;
				}
				
				sb_map::light_color_area* lca = (sb_map::light_color_area*)object_selection.get_ptr();
				const uint3 new_max_pos(int3(lca->max_pos) + dir);
				const uint3 map_max_pos(sb_map::chunk_extent * active_map->get_chunk_count());
				lca->max_pos.x = new_max_pos.x < map_max_pos.x ? new_max_pos.x : map_max_pos.x;
				lca->max_pos.y = new_max_pos.y < map_max_pos.y ? new_max_pos.y : map_max_pos.y;
				lca->max_pos.z = new_max_pos.z < map_max_pos.z ? new_max_pos.z : map_max_pos.z;
				lca->max_pos.x = lca->max_pos.x < lca->min_pos.x ? lca->min_pos.x : lca->max_pos.x;
				lca->max_pos.y = lca->max_pos.y < lca->min_pos.y ? lca->min_pos.y : lca->max_pos.y;
				lca->max_pos.z = lca->max_pos.z < lca->min_pos.z ? lca->min_pos.z : lca->max_pos.z;
				
				active_map->update_light_colors();
				e->release_gl_context();
			});
			next_height();
			
			for(size_t col_idx = 0; col_idx < 3; col_idx++) {
				light_color_area_ui.light_color_sliders[col_idx] = ui->add<gui_slider>(data_size,
																					   float2(column_offsets[1], next_height()));
				light_color_area_ui.light_color_sliders[col_idx]->set_knob_position(1.0f);
				workspace_wnd->add_child(light_color_area_ui.light_color_sliders[col_idx]);
				light_color_area_ui.light_color_sliders[col_idx]->add_handler([col_idx,this](GUI_EVENT, gui_object&) {
					e->acquire_gl_context();
					const float3 color(light_color_area_ui.light_color_sliders[0]->get_knob_position(),
									   light_color_area_ui.light_color_sliders[1]->get_knob_position(),
									   light_color_area_ui.light_color_sliders[2]->get_knob_position());
					sb_map::light_color_area* lca = (sb_map::light_color_area*)object_selection.get_ptr();
					lca->color = color;
					active_map->update_light_colors();
					e->release_gl_context();
				}, GUI_EVENT::SLIDER_MOVE);
			}
		}
		break;
		case OBJECT_TYPE::AI_WAYPOINT: {
			//// text
			ai_waypoint_ui.identifier_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			ai_waypoint_ui.next_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			ai_waypoint_ui.position_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
			ai_waypoint_ui.identifier_text->set_label("Identifier");
			ai_waypoint_ui.next_text->set_label("Next");
			ai_waypoint_ui.position_text->set_label("Position");
			ai_waypoint_ui.identifier_text->set_shade(true);
			ai_waypoint_ui.next_text->set_shade(true);
			ai_waypoint_ui.position_text->set_shade(true);
			workspace_wnd->add_child(ai_waypoint_ui.identifier_text);
			workspace_wnd->add_child(ai_waypoint_ui.next_text);
			workspace_wnd->add_child(ai_waypoint_ui.position_text);
			
			////
			reset_height();
			ai_waypoint_ui.identifier_input = ui->add<gui_input_box>(data_size, float2(column_offsets[1], next_height()));
			ai_waypoint_ui.next_button = ui->add<gui_pop_up_button>(data_size, float2(column_offsets[1], next_height()));
			workspace_wnd->add_child(ai_waypoint_ui.identifier_input);
			workspace_wnd->add_child(ai_waypoint_ui.next_button);
			
			create_xyz_buttons(height, workspace_wnd,
							   ai_waypoint_ui.position_buttons, [this](GUI_EVENT, gui_object& obj)
			{
				e->acquire_gl_context();
				int3 dir;
				for(size_t i = 0; i < 6; i++) {
					if(ai_waypoint_ui.position_buttons[i] == &obj) {
						dir[i/2] = ((i % 2) == 0) ? 1 : -1;
						break;
					}
				}
				if(dir.is_null()) {
					e->release_gl_context();
					return;
				}
				
				sb_map::ai_waypoint* wp = (sb_map::ai_waypoint*)object_selection.get_ptr();
				const uint3 new_pos(int3(wp->position) + dir);
				if(active_map->is_valid_position(new_pos)) {
					wp->position = new_pos;
				}
				
				e->release_gl_context();
			});
			next_height();
			
			ai_waypoint_ui.identifier_input->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				sb_map::ai_waypoint* wp = (sb_map::ai_waypoint*)object_selection.get_ptr();
				wp->identifier = ai_waypoint_ui.identifier_input->get_input();
				ui->set_active_object(nullptr); // make input box inactive
				e->release_gl_context();
			}, GUI_EVENT::INPUT_BOX_ENTER);
			
			ai_waypoint_ui.next_button->add_handler([this](GUI_EVENT, gui_object&) {
				e->acquire_gl_context();
				sb_map::ai_waypoint* wp = (sb_map::ai_waypoint*)object_selection.get_ptr();
				const string& next_str(ai_waypoint_ui.next_button->get_selected_item()->first);
				if(next_str == "None") {
					wp->next = nullptr;
				}
				else {
					wp->next = nullptr;
					for(const auto& ai_wp : active_map->get_ai_waypoints()) {
						if(ai_wp->identifier == next_str) {
							wp->next = ai_wp;
							break;
						}
					}
				}
				e->release_gl_context();
			}, GUI_EVENT::POP_UP_BUTTON_SELECT);
		}
		break;
		case OBJECT_TYPE::__MAX_OBJECT_TYPE: return;
	}
	update_object_ui();
	
	// completely disable camera when an object was selected
	if(type != OBJECT_TYPE::__MAX_OBJECT_TYPE) {
		switch_input_mode(false);
	}
}

void editor::update_object_ui() {
	if(workspace_wnd == nullptr) return;
	
	switch(object_selection.type) {
		case OBJECT_TYPE::SOUND: {
			audio_3d* snd = (audio_3d*)object_selection.get_ptr();
			const string& identifier(snd->get_identifier());
			const size_t point_pos(identifier.find("."));
			sound_ui.identifier_input->set_input(identifier.substr(point_pos+1, identifier.size()-point_pos-1));
			sound_ui.file_button->set_selected_item(identifier.substr(0, point_pos));
			
			sound_ui.play_on_load_button->set_toggled(snd->get_play_on_load());
			sound_ui.loop_button->set_toggled(snd->is_looping());
			
			sound_ui.volume_input->set_input(float2string(snd->get_volume()));
			sound_ui.ref_dist_input->set_input(float2string(snd->get_reference_distance()));
			sound_ui.rolloff_input->set_input(float2string(snd->get_rolloff_factor()));
			sound_ui.max_dist_input->set_input(float2string(snd->get_max_distance()));
		}
		break;
		case OBJECT_TYPE::MAP_LINK: {
			sb_map::map_link* ml = (sb_map::map_link*)object_selection.get_ptr();
			map_link_ui.dst_button->set_selected_item(ml->dst_map_name);
			map_link_ui.identifier_input->set_input(ml->identifier);
			map_link_ui.enabled_button->set_toggled(ml->enabled);
		}
		break;
		case OBJECT_TYPE::TRIGGER: {
			sb_map::trigger* trgr = (sb_map::trigger*)object_selection.get_ptr();
			trigger_ui.type_button->set_selected_item((unsigned int)trgr->type);
			trigger_ui.sub_type_button->set_selected_item((unsigned int)trgr->sub_type);
			trigger_ui.identifier_input->set_input(trgr->identifier);
			
			trigger_ui.on_load_button->clear();
			trigger_ui.on_trigger_button->clear();
			trigger_ui.on_untrigger_button->clear();
			fill_pop_up_button(trigger_ui.on_load_button, { "None" });
			fill_pop_up_button(trigger_ui.on_trigger_button, { "None" });
			fill_pop_up_button(trigger_ui.on_untrigger_button, { "None" });
			if(trgr->scr != nullptr) {
				const string& script_name(trgr->scr->get_filename());
				const size_t slash_pos(script_name.rfind("/"));
				if(slash_pos != string::npos) {
					trigger_ui.script_button->set_selected_item(script_name.substr(slash_pos+1, script_name.size()-slash_pos-1));
				}
				const auto& functions(trgr->scr->get_functions());
				vector<string> sorted_functions;
				for(const auto& func : functions) sorted_functions.emplace_back(func.first);
				sort(begin(sorted_functions), end(sorted_functions));
				trigger_ui.on_load_button->set_selected_item(0);
				trigger_ui.on_trigger_button->set_selected_item(0);
				trigger_ui.on_untrigger_button->set_selected_item(0);
				for(const auto& func : sorted_functions) {
					trigger_ui.on_load_button->add_item(func, func);
					trigger_ui.on_trigger_button->add_item(func, func);
					trigger_ui.on_untrigger_button->add_item(func, func);
				}
				if(!trgr->on_load.empty()) trigger_ui.on_load_button->set_selected_item(trgr->on_load);
				if(!trgr->on_trigger.empty()) trigger_ui.on_trigger_button->set_selected_item(trgr->on_trigger);
				if(!trgr->on_untrigger.empty()) trigger_ui.on_untrigger_button->set_selected_item(trgr->on_untrigger);
			}
			else {
				trigger_ui.script_button->set_selected_item(0);
				trigger_ui.on_load_button->set_selected_item(0);
				trigger_ui.on_trigger_button->set_selected_item(0);
				trigger_ui.on_untrigger_button->set_selected_item(0);
			}
			
			for(unsigned int i = 0; i < 6; i++) {
				trigger_ui.facing_buttons[i]->set_toggled(((1 << i) & (unsigned int)trgr->facing) != 0);
			}
			
			trigger_ui.weight_input->set_input("");
			trigger_ui.weight_input->set_enabled(false);
			trigger_ui.intensity_text->set_label("Intensity");
			trigger_ui.intensity_input->set_input("");
			trigger_ui.intensity_input->set_enabled(false);
			trigger_ui.timer_input->set_input("");
			trigger_ui.timer_input->set_enabled(false);
			
			if(trgr->type == TRIGGER_TYPE::WEIGHT) {
				trigger_ui.weight_input->set_input(float2string(trgr->weight));
				trigger_ui.weight_input->set_enabled(true);
			}
			else if(trgr->type == TRIGGER_TYPE::LIGHT) {
				const string intensity_str(float2string(active_map->light_intensity_for_position(trgr->position)));
				trigger_ui.intensity_text->set_label("Intensity ("+(intensity_str.size() > 8 ?
																	intensity_str.substr(0, 8) : intensity_str)+")");
				trigger_ui.intensity_input->set_input(float2string(trgr->intensity));
				trigger_ui.intensity_input->set_enabled(true);
			}
			
			if(trgr->sub_type == TRIGGER_SUB_TYPE::TIMED) {
				trigger_ui.timer_input->set_input(uint2string(trgr->time));
				trigger_ui.timer_input->set_enabled(true);
			}
		}
		break;
		case OBJECT_TYPE::LIGHT_COLOR_AREA: {
			sb_map::light_color_area* lca = (sb_map::light_color_area*)object_selection.get_ptr();
			for(size_t col_idx = 0; col_idx < 3; col_idx++) {
				light_color_area_ui.light_color_sliders[col_idx]->set_knob_position(lca->color[col_idx]);
			}
		}
		break;
		case OBJECT_TYPE::AI_WAYPOINT: {
			sb_map::ai_waypoint* wp = (sb_map::ai_waypoint*)object_selection.get_ptr();
			ai_waypoint_ui.identifier_input->set_input(wp->identifier);
			
			ai_waypoint_ui.next_button->clear();
			ai_waypoint_ui.next_button->add_item("None", "None");
			for(const auto& ai_wp : active_map->get_ai_waypoints()) {
				ai_waypoint_ui.next_button->add_item(ai_wp->identifier, ai_wp->identifier);
			}
			ai_waypoint_ui.next_button->set_selected_item(wp->next == nullptr ? "None" : wp->next->identifier);
		}
		break;
		default:
			break;
	}
}

void editor::open_settings_ui() {
	if(workspace_wnd == nullptr) return;
	close_workspace_ui(); // close old ui
	
	//
	float height = 0.0f;
	const auto next_height = [&height]() -> float {
		const float ret { height };
		height += element_height + row_offset;
		return ret;
	};
	const auto reset_height = [&height]() { height = row_offset; };
	reset_height();
	
	//
	map_settings_ui.name_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
	map_settings_ui.player_position_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
	map_settings_ui.player_orientation_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
	map_settings_ui.background_audio_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
	map_settings_ui.chunks_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
	next_height(); // chunks buttons
	map_settings_ui.light_color_text = ui->add<gui_text>(text_size, float2(column_offsets[0], next_height()));
	map_settings_ui.name_text->set_label("Map Name");
	map_settings_ui.player_position_text->set_label("Player Position");
	map_settings_ui.player_orientation_text->set_label("Player Orientation");
	map_settings_ui.background_audio_text->set_label("Background Music");
	map_settings_ui.chunks_text->set_label("Chunks");
	map_settings_ui.light_color_text->set_label("Default Light Color");
	map_settings_ui.name_text->set_shade(true);
	map_settings_ui.player_position_text->set_shade(true);
	map_settings_ui.player_orientation_text->set_shade(true);
	map_settings_ui.background_audio_text->set_shade(true);
	map_settings_ui.chunks_text->set_shade(true);
	map_settings_ui.light_color_text->set_shade(true);
	workspace_wnd->add_child(map_settings_ui.name_text);
	workspace_wnd->add_child(map_settings_ui.player_position_text);
	workspace_wnd->add_child(map_settings_ui.player_orientation_text);
	workspace_wnd->add_child(map_settings_ui.background_audio_text);
	workspace_wnd->add_child(map_settings_ui.chunks_text);
	workspace_wnd->add_child(map_settings_ui.light_color_text);
	
	//
	reset_height();
	map_settings_ui.name_input = ui->add<gui_input_box>(data_size, float2(column_offsets[1], next_height()));
	workspace_wnd->add_child(map_settings_ui.name_input);
	map_settings_ui.name_input->add_handler([this](GUI_EVENT, gui_object&) {
		e->acquire_gl_context();
		active_map->set_name(map_settings_ui.name_input->get_input());
		ui->set_active_object(nullptr); // make input box inactive
		e->release_gl_context();
	}, GUI_EVENT::INPUT_BOX_ENTER);
	
	create_xyz_buttons(height, workspace_wnd,
					   map_settings_ui.player_position_buttons,
					   [this](GUI_EVENT, gui_object& obj)
	{
		e->acquire_gl_context();
		int3 dir;
		for(size_t i = 0; i < 6; i++) {
			if(map_settings_ui.player_position_buttons[i] == &obj) {
				dir[i/2] = ((i % 2) == 0) ? 1 : -1;
				break;
			}
		}
		if(dir.is_null()) {
			e->release_gl_context();
			return;
		}
		
		const uint3 new_pos(int3(active_map->get_player_start()) + dir);
		if(active_map->is_valid_position(new_pos)) {
			active_map->set_player_start(new_pos);
		}
		
		e->release_gl_context();
	});
	next_height();
	
	map_settings_ui.player_orientation_button = ui->add<gui_pop_up_button>(data_size, float2(column_offsets[1], next_height()));
	static const vector<pair<string, string>> orientations {
		{
			{ "0,-1", "0° (-Z)" },
			{ "1,-1", "45° (+X / -Z)" },
			{ "1,0", "90° (+X)" },
			{ "1,1", "135° (+X / +Z)" },
			{ "0,1", "180° (+Z)" },
			{ "-1,1", "225° (-X / +Z)" },
			{ "-1,0", "270° (-X)" },
			{ "-1,-1", "315° (-X / -Z)" },
		}
	};
	for(const auto& orientation : orientations) {
		map_settings_ui.player_orientation_button->add_item(orientation.first, orientation.second);
	}
	workspace_wnd->add_child(map_settings_ui.player_orientation_button);
	map_settings_ui.player_orientation_button->add_handler([this](GUI_EVENT, gui_object&) {
		e->acquire_gl_context();
		const string& vec_str(map_settings_ui.player_orientation_button->get_selected_item()->first);
		const auto tokens = core::tokenize(vec_str, ',');
		active_map->set_player_rotation(float3(string2float(tokens[0]), 0.0f, string2float(tokens[1])));
		e->release_gl_context();
	}, GUI_EVENT::POP_UP_BUTTON_SELECT);
	
	map_settings_ui.background_audio_button = ui->add<gui_pop_up_button>(data_size, float2(column_offsets[1], next_height()));
	const auto audio_list(core::get_file_list(e->data_path("music/"), "mp3"));
	set<string> bg_music_list;
	for(const auto& audio_file : audio_list) {
		if(audio_file.first.substr(0, 3) == "bg_") {
			const string identifier = core::str_to_upper(audio_file.first.substr(3, audio_file.first.size() - 7));
			bg_music_list.insert(identifier);
		}
	}
	map_settings_ui.background_audio_button->add_item("None", "None");
	for(const auto& bg_music : bg_music_list) {
		map_settings_ui.background_audio_button->add_item(bg_music, bg_music);
	}
	workspace_wnd->add_child(map_settings_ui.background_audio_button);
	map_settings_ui.background_audio_button->add_handler([this](GUI_EVENT, gui_object&) {
		e->acquire_gl_context();
		
		const auto& cur_bg_music = active_map->get_background_music();
		const string identifier(map_settings_ui.background_audio_button->get_selected_item()->first);
		if(cur_bg_music != nullptr && cur_bg_music->get_identifier() == identifier+".0") {
			e->release_gl_context();
			return;
		}
		
		// remove old bg music
		if(cur_bg_music != nullptr) {
			active_map->remove_background_music();
		}
		
		// add new bg music (if not "None")
		const string filename("bg_"+core::str_to_lower(identifier)+".mp3");
		if(identifier != "None") {
			if(as->load_file(e->data_path("music/"+filename), identifier) == nullptr) {
				throw a2e_exception("failed to load background audio: "+filename+" ("+identifier+")");
			}
			
			audio_background* bg = ac->add_audio_background(identifier, "0");
			if(bg == nullptr) {
				a2e_error("failed to create background audio for: %s.0", identifier);
				e->release_gl_context();
				return;
			}
			bg->set_volume(1.0f);
			bg->play();
			active_map->set_background_music(bg);
		}
		
		e->release_gl_context();
	}, GUI_EVENT::POP_UP_BUTTON_SELECT);
	
	const float data_size_third_x(data_size.x / 3.0f);
	const float2 data_size_third(data_size_third_x, data_size.y);
	for(size_t size_index = 0; size_index < 3; size_index++) {
		map_settings_ui.chunks_input[size_index] = ui->add<gui_input_box>(data_size_third,
																		  float2(column_offsets[1] + data_size_third_x * float(size_index),
																				 height));
		workspace_wnd->add_child(map_settings_ui.chunks_input[size_index]);
		map_settings_ui.chunks_input[size_index]->add_handler([this, size_index](GUI_EVENT, gui_object&) {
			e->acquire_gl_context();
			
			int3 new_chunk_count(active_map->get_chunk_count());
			new_chunk_count[size_index] = string2uint(map_settings_ui.chunks_input[size_index]->get_input());
			new_chunk_count.max(int3(1)); // there must be at least one chunk on each axis
			active_map->resize(new_chunk_count);
			
			ui->set_active_object(nullptr); // make input box inactive
			update_settings_ui();
			e->release_gl_context();
		}, GUI_EVENT::INPUT_BOX_ENTER);
	}
	next_height();
	create_xyz_buttons(height, workspace_wnd,
					   map_settings_ui.chunks_button,
					   [this](GUI_EVENT, gui_object& obj) {
		e->acquire_gl_context();
		int3 dir;
		for(size_t i = 0; i < 6; i++) {
			if(map_settings_ui.chunks_button[i] == &obj) {
				dir[i/2] = ((i % 2) == 0) ? 1 : -1;
				break;
			}
		}
		if(dir.is_null()) {
			e->release_gl_context();
			return;
		}
		
		int3 new_chunk_count(int3(active_map->get_chunk_count()) + dir);
		new_chunk_count.max(int3(1)); // there must be at least one chunk on each axis
		active_map->resize(new_chunk_count);
		update_settings_ui();
		e->release_gl_context();
	});
	next_height();
	
	//
	for(size_t col_idx = 0; col_idx < 3; col_idx++) {
		map_settings_ui.light_color_sliders[col_idx] = ui->add<gui_slider>(data_size, float2(column_offsets[1], next_height()));
		map_settings_ui.light_color_sliders[col_idx]->set_knob_position(1.0f);
		workspace_wnd->add_child(map_settings_ui.light_color_sliders[col_idx]);
		map_settings_ui.light_color_sliders[col_idx]->add_handler([col_idx,this](GUI_EVENT, gui_object&) {
			e->acquire_gl_context();
			const float3 color(map_settings_ui.light_color_sliders[0]->get_knob_position(),
							   map_settings_ui.light_color_sliders[1]->get_knob_position(),
							   map_settings_ui.light_color_sliders[2]->get_knob_position());
			active_map->set_default_light_color(color);
			active_map->update_light_colors();
			e->release_gl_context();
		}, GUI_EVENT::SLIDER_MOVE);
	}
	
	//
	update_settings_ui();
}

void editor::update_settings_ui() {
	if(workspace_wnd == nullptr) return;
	if(object_selection.type != OBJECT_TYPE::__MAX_OBJECT_TYPE) return;
	
	//
	map_settings_ui.name_input->set_input(active_map->get_name());
	
	// -> closest match
	const float3& rot = active_map->get_player_rotation();
	const float angle = fabsf((rot.x < 0.0f ? 360.0f : 0.0f) -
							  RAD2DEG(float3(0.0f, 0.0f, -1.0f).angle(rot.normalized())));
	const size_t rot_index = ((size_t)roundf(angle / 45.0f)) % 8;
	map_settings_ui.player_orientation_button->set_selected_item(rot_index);
	
	const auto& music = active_map->get_background_music();
	if(music != nullptr) {
		string identifier = music->get_identifier();
		const size_t point_pos(identifier.find("."));
		if(point_pos != string::npos) {
			identifier = identifier.substr(0, point_pos);
		}
		map_settings_ui.background_audio_button->set_selected_item(identifier);
	}
	else map_settings_ui.background_audio_button->set_selected_item("None");
	
	for(size_t i = 0; i < 3; i++) {
		map_settings_ui.chunks_input[i]->set_input(uint2string(active_map->get_chunk_count()[i]));
	}
	
	const float3& color = active_map->get_default_light_color();
	for(size_t col_idx = 0; col_idx < 3; col_idx++) {
		map_settings_ui.light_color_sliders[col_idx]->set_knob_position(color[col_idx]);
	}
}

void editor::close_workspace_ui() {
	if(workspace_wnd == nullptr) return;
	workspace_wnd->clear();
}
