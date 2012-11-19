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

#include "script_handler.h"
#include "script.h"
#include <engine.h>

script_handler::script_handler() {
}

script_handler::~script_handler() {
	for(const auto& scr : scripts) {
		delete scr.second;
	}
}

script* script_handler::load_script(const string& filename) {
	if(scripts.count(filename)) return scripts.at(filename); // already loaded
	script* scr = new script(e->data_path("scripts/"+filename));
	scripts.insert(make_pair(filename, scr));
	return scr;
}

script* script_handler::reload_script(const string& filename) {
	const auto iter = scripts.find(filename);
	if(iter != scripts.end()) {
		iter->second->reload();
		return iter->second;
	}
	return load_script(filename);
}

script* script_handler::get_script(const string& filename) const {
	if(scripts.count(filename) == 0) return nullptr;
	return scripts.at(filename);
}
