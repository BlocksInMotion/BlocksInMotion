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

#ifndef __SB_CONTROLS_H__
#define __SB_CONTROLS_H__

#include "sb_global.h"

class controls {
public:
	typedef unsigned long long int action_event;
	enum class ACTION : unsigned int {
		NONE,
		
		PHYSICSGUN_PULL,
		PHYSICSGUN_PUSH,
		PHYSICSGUN_GRAVITY,
		
		MOVE_FORWARDS,
		MOVE_BACKWARDS,
		MOVE_LEFT,
		MOVE_RIGHT,
		
		JUMP,
		
		CONSOLE,
		
		__MAX_ACTION
	};
	enum class MOUSE_EVENT_TYPE : unsigned long long int {
		LEFT = 0x100000000ULL,
		RIGHT = 0x200000000ULL,
		MIDDLE = 0x300000000ULL,
	};
	
	static void set(const action_event& evt, const ACTION& action, const bool primary = true);
	static void clear(const ACTION& action);
	static ACTION get(const action_event& evt);
	static pair<action_event, action_event> get(const ACTION& action);
	
	static string name(const ACTION& action);
	static string name(const action_event& evt);
	
	static pair<ACTION, bool> action_from_event(const EVENT_TYPE& type, const shared_ptr<event_object>& obj);
	
	//
	static void load();
	static void save();
	
protected:
	// action -> <primary event, secondary event>
	static unordered_map<ACTION, pair<action_event, action_event>> mapping;
	
	controls() = delete;
	~controls() = delete;
	controls& operator=(const controls&) = delete;
};
enum_class_hash(controls::ACTION)

#endif
