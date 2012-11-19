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

#ifndef __SB_MAP_STORAGE_H__
#define __SB_MAP_STORAGE_H__

#include "sb_global.h"

class sb_map;
class map_storage {
public:
	map_storage() = delete;
	~map_storage() = delete;
	
	enum class DATA_TYPES : unsigned int {
		MAP_DATA			= 'MAPD',
		AUDIO_BACKGROUND	= 'AUBG',
		AUDIO_3D			= 'AU3D',
		MAP_LINK			= 'LINK',
		TRIGGER				= 'TRGR',
		LIGHT_COLOR_AREA	= 'LICA',
		AI_WAYPOINT			= 'AIWP',
	};
	static const unsigned int header_magic;
	
	// note: map filename is relative to data path
	static sb_map* load(const string& filename);
	static bool save(const string& filename, const sb_map& level);
	
protected:
	typedef std::function<void(DATA_TYPES type, std::function<void()>)> struct_writer;
	typedef std::function<bool(file_io& file, sb_map& level)> load_function;
	typedef std::function<bool(file_io& file, const sb_map& level, struct_writer writer)> save_function;
	
	static const unordered_map<unsigned int, load_function> loaders;
	static const unordered_map<unsigned int, save_function> savers;
	static const unordered_map<unsigned int, unsigned int> data_versions;
	
	// loaders:
	static bool load_map_data(file_io& file, sb_map& level);
	static bool load_audio_background(file_io& file, sb_map& level);
	static bool load_audio_3d(file_io& file, sb_map& level);
	static bool load_map_link(file_io& file, sb_map& level);
	static bool load_trigger(file_io& file, sb_map& level);
	static bool load_light_color_area(file_io& file, sb_map& level);
	static bool load_ai_waypoint(file_io& file, sb_map& level);
	
	// savers:
	static bool save_map_data(file_io& file, const sb_map& level, struct_writer writer);
	static bool save_audio_background(file_io& file, const sb_map& level, struct_writer writer);
	static bool save_audio_3d(file_io& file, const sb_map& level, struct_writer writer);
	static bool save_map_link(file_io& file, const sb_map& level, struct_writer writer);
	static bool save_trigger(file_io& file, const sb_map& level, struct_writer writer);
	static bool save_light_color_area(file_io& file, const sb_map& level, struct_writer writer);
	static bool save_ai_waypoint(file_io& file, const sb_map& level, struct_writer writer);

};

#endif
