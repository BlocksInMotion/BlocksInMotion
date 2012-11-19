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

#ifndef __SB_MAP_H__
#define __SB_MAP_H__

#include "sb_global.h"
#include <atomic>

// for convenience and forward-declarability(tm), make these global:
enum class BLOCK_MATERIAL : unsigned int {
	NONE,
	INDESTRUCTIBLE,
	METAL,
	__PLACEHOLDER_0,
	MAGNET,
	LIGHT,
	__PLACEHOLDER_1,
	ACID,
	__PLACEHOLDER_2,
	__PLACEHOLDER_3,
	SPRING,
	SPAWNER,
	__MAX_BLOCK_MATERIAL
};

enum class BLOCK_FACE : unsigned int {
	INVALID		= 0,
	RIGHT		= (1 << 0),
	LEFT		= (1 << 1),
	TOP			= (1 << 2),
	BOTTOM		= (1 << 3),
	BACK		= (1 << 4),
	FRONT		= (1 << 5),
	ALL			= (RIGHT | LEFT | TOP | BOTTOM | BACK | FRONT)
};

enum class TRIGGER_TYPE : unsigned int {
	NONE,
	PUSH,
	WEIGHT, // also: force/velocity
	LIGHT,
};
enum class TRIGGER_SUB_TYPE : unsigned int {
	NONE,
	TIMED,
};

struct rigid_info;
class rigid_body;
class soft_body;
class light;
class audio_background;
class audio_3d;
class a2emodel;
class a2ematerial;
class ai_entity;
class script;
class weight_slider;
enum class GAME_STATUS;
class sb_map {
public:
	sb_map(const string& filename);
	~sb_map();
	
	void run();
	
	//
	static constexpr unsigned int map_version = 2;
	struct block_data {
		BLOCK_MATERIAL material = BLOCK_MATERIAL::NONE;
	};
	static constexpr size_t chunk_extent = 16;
	static constexpr size_t blocks_per_chunk = chunk_extent*chunk_extent*chunk_extent;
	// stored in X*Z*Y order
	typedef array<block_data, blocks_per_chunk> chunk;
	
	// block/chunk update/change functions
	void update(const unsigned int& chunk_index, const uint3& local_position, const BLOCK_MATERIAL& mat);
	void update(const uint3& chunk_position, const uint3& local_position, const BLOCK_MATERIAL& mat);
	void update(const chunk& chnk, const uint3& local_position, const BLOCK_MATERIAL& mat);
	void update(const uint3& global_position, const BLOCK_MATERIAL& mat);
	void update(const pair<uint3, uint3>& global_min_max_position, const BLOCK_MATERIAL& mat);
	
	const vector<chunk>& get_chunks() const;
	const chunk& get_chunk(const unsigned int& chunk_index) const;
	const block_data& get_block(const unsigned int& chunk_index, const unsigned int& block_index) const;
	const block_data& get_block(const uint3& global_position) const;
	
	// block/chunk position and index conversion
	uint3 chunk_index_to_position(const unsigned int& chunk_index) const {
		return uint3(chunk_index % chunk_count.x,
					 chunk_index / (chunk_count.x * chunk_count.z),
					 (chunk_index / chunk_count.x) % chunk_count.z);
	}
	unsigned int chunk_position_to_index(const uint3& chunk_position) const {
		return chunk_position.y * (chunk_count.x * chunk_count.z) + chunk_position.z * chunk_count.x + chunk_position.x;
	}
	static uint3 block_index_to_position(const unsigned int& block_index) {
		return uint3(block_index % chunk_extent,
					 block_index / (chunk_extent*chunk_extent),
					 (block_index / chunk_extent) % chunk_extent);
	}
	static unsigned int block_position_to_index(const uint3& block_position) {
		return block_position.y * (chunk_extent*chunk_extent) + block_position.z * chunk_extent + block_position.x;
	}
	bool is_valid_position(const uint3& global_position) const;
	
	// org map functions
	void set_name(const string& name);
	const string& get_name() const;
	
	void set_filename(const string& filename);
	const string& get_filename() const;
	
	void resize(const uint3& chunk_count);
	const uint3& get_chunk_count() const;
	
	void set_player_start(const uint3& player_start);
	const uint3& get_player_start() const;
	
	void set_player_rotation(const float3& player_rotation);
	const float3& get_player_rotation() const;
	
	void set_default_light_color(const float3& color);
	const float3& get_default_light_color() const;
	
	// physics functions
	rigid_body* make_dynamic(const unsigned int& chunk_index, const unsigned int& block_index);
	rigid_body* resolve_dynamic(const uint3& global_position);
	
	const unordered_map<rigid_body*, BLOCK_MATERIAL>& get_dynamic_bodies() const;
	void update_dynamic(rigid_body* body, const BLOCK_MATERIAL& block_material);
	
	const vector<ai_entity*>& get_ai_entities() const;
	
	// audio functions
	void set_background_music(audio_background* bg);
	audio_background* get_background_music() const;
	void remove_background_music();
	
	void add_sound(audio_3d* sound);
	const set<audio_3d*>& get_sounds() const;
	audio_3d* get_sound(const string& identifier) const;
	void remove_sound(audio_3d* sound);
	
	// lighting functions
	struct light_color_area {
		uint3 min_pos;
		uint3 max_pos;
		float3 color;
	};
	void add_light_color_area(light_color_area* lca);
	void remove_light_color_area(light_color_area* lca);
	const vector<light_color_area*>& get_light_color_areas() const;
	void update_light_colors();
	
	light* get_light(const uint3& global_position) const;
	const vector<unordered_map<unsigned int, light*>>& get_lights() const;
	
	float light_intensity_for_position(const uint3& global_position) const;
	
	// misc functions
	audio_3d* play_sound(const string& identifier, const float3& position, const float volume = 1.0f, const bool is_looping = false, const bool can_be_killed = true) const;
	void stop_sound(const string& identifier) const;
	
	void kill_player();
	
	struct map_link {
		string dst_map_name;
		string identifier;
		vector<uint3> positions;
		bool enabled;
	};
	void add_map_link(map_link* ml);
	void remove_map_link(map_link* ml);
	const pair<bool, map_link*>& is_map_change() const;
	const vector<map_link*>& get_map_links() const;
	map_link* get_map_link(const string& identifier) const;
	
	void handle_block_click(const pair<uint3, BLOCK_FACE>& clicked_block, bool& activated);
	
	static void change_map(const string map_filename, const GAME_STATUS status);
	
	// trigger functions
	struct trigger {
		string identifier;
		TRIGGER_TYPE type;
		TRIGGER_SUB_TYPE sub_type;
		uint3 position;
		BLOCK_FACE facing;
		script* scr;
		string on_load;
		string on_trigger;
		string on_untrigger;
		
		// dependent data (oo is overrated):
		float weight;
		float intensity;
		unsigned int time;
		
		// runtime modifiable vars:
		struct state_struct {
			atomic<unsigned int> active; // just pretend this is a bool
			unsigned int timer { 0 };
			weight_slider* slider { nullptr };
			state_struct() { active.store(0); }
			state_struct(state_struct&& state_) { active.store(state_.active); }
		} state;
		
		void activate(sb_map* cur_map);
		void deactivate(sb_map* cur_map);
	};
	void add_trigger(trigger* trgr);
	void remove_trigger(trigger* trgr);
	const vector<trigger*> get_triggers() const;
	trigger* get_trigger(const string& identifier) const;
	
	// ai functions
	struct ai_waypoint {
		string identifier; // identification in the editor and map file
		ai_waypoint* next; // pointer to the next waypoint, nullptr if none
		uint3 position; // global position of the waypoint
	};
	void add_ai_waypoint(ai_waypoint* wp);
	void remove_ai_waypoint(ai_waypoint* wp);
	const vector<ai_waypoint*>& get_ai_waypoints() const;
	ai_waypoint* get_ai_waypoint(const string& identifier) const;
	ai_entity* add_ai_entity(const uint3& position);
	
	// render data
	struct chunk_render_data {
	public:
		float3 offset;
		GLuint ubo;
		chunk_render_data(const float3& offset_, array<unsigned int, blocks_per_chunk>& render_data);
		chunk_render_data(chunk_render_data&& crd);
		~chunk_render_data();
	};
	const vector<chunk_render_data>& get_render_chunks() const;
	
	void update_dynamic_render_data();
	const pair<GLuint, size_t> get_dynamic_render_data() const;
	
	static unsigned int remap_material(const BLOCK_MATERIAL& mat);
	
protected:
	string filename;
	string name = "";
	uint3 chunk_count;
	uint3 player_start;
	float3 player_rotation;
	vector<chunk> chunks;
	vector<chunk_render_data> render_chunks;
	
	const rigid_info* block_rinfo;
	vector<unordered_map<unsigned int, rigid_body*>> static_bodies;
	vector<unordered_map<unsigned int, rigid_body*>> dynamic_body_field;
	unordered_map<rigid_body*, BLOCK_MATERIAL> dynamic_bodies;
	array<matrix4f, 1024> dynamic_render_data;
	size_t dynamic_render_data_size;
	GLuint dynamic_bodies_ubo = 0;

	vector<unordered_map<unsigned int, light*>> lights;
	vector<light_color_area*> light_color_areas;
	float3 default_light_color = float3(1.0f);
	float3 compute_light_color_for_position(const uint3& global_position) const;
	
	// non-block-bound objects
	audio_background* background_music = nullptr;
	set<audio_3d*> env_sounds;
	vector<map_link*> map_links;
	
	// misc
	pair<bool, map_link*> map_change { false, nullptr };

	struct spring {
		const uint3 position;
		const float3 direction;
		rigid_info* info;
		rigid_body* body;
		float scale; // [0, 1]
		float state; // pos: extending, neg: retracting
		unsigned int timer;
	};
	vector<spring*> springs;
	void add_spring(const uint3& position, const float3& direction);
	
	struct spawner {
		const uint3 position;
		const size_t max_spawns; // min/max?
		set<ai_entity*> spawns;
	};
	vector<spawner*> spawners;
	vector<ai_entity*> entities;
	void add_spawner(const uint3& position);
	void remove_spawner(const uint3& position);
	
	// triggers
	vector<trigger*> triggers;
	
	// ai
	vector<ai_waypoint*> ai_waypoints;
	
	//
	event::handler evt_handler_fnctr;
	bool event_handler(EVENT_TYPE type, shared_ptr<event_object> obj);

};

#endif
