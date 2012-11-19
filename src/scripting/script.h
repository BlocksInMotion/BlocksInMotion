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

#ifndef __SB_SCRIPT_H__
#define __SB_SCRIPT_H__

#include "sb_global.h"

class sb_map;
class script {
public:
	script(const string& filename);
	~script();
	
	void reload();
	
	struct script_function {
		// note: lines are already tokenized
		vector<vector<string>> lines;
		script_function() : lines() {}
		script_function(script_function&& sf) : lines(sf.lines) {}
	};
	
	const string& get_filename() const;
	
	enum class COMMAND : unsigned int {
		NOP,
		ADD_BLOCK,
		DELETE_BLOCK,
		MODIFY_BLOCK,
		TOGGLE_BLOCK,
		MAKE_DYNAMIC,
		CALL,
		CALL_SCRIPT,
		ACTIVATE,
		DEACTIVATE,
		TRIGGER,
		PLAY_SOUND,
		KILL_LIGHTS,
		LIGHT_COLOR,
		DELETE_TRIGGER,
		MOVE_TRIGGER,
	};
	
	void execute(sb_map* cur_map, const string& identifier, const uint3 position = uint3(~0u)) const;
	const unordered_map<string, script_function>& get_functions() const;
	
protected:
	const string filename;
	
	// <identifier, commands/script>
	unordered_map<string, script_function> functions;
	
	void load(const string& filename);

};

#endif
