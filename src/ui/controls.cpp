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

#include "controls.h"
#include <engine.h>
#include <core/xml.h>

unordered_map<controls::ACTION, pair<controls::action_event, controls::action_event>> controls::mapping;
static const unordered_map<controls::ACTION, string> conf_mapping {
	{
		{ controls::ACTION::PHYSICSGUN_PULL, "pg_pull" },
		{ controls::ACTION::PHYSICSGUN_PUSH, "pg_push" },
		{ controls::ACTION::PHYSICSGUN_GRAVITY, "pg_gravity" },
		{ controls::ACTION::MOVE_FORWARDS, "forwards" },
		{ controls::ACTION::MOVE_BACKWARDS, "backwards" },
		{ controls::ACTION::MOVE_LEFT, "left" },
		{ controls::ACTION::MOVE_RIGHT, "right" },
		{ controls::ACTION::JUMP, "jump" },
		{ controls::ACTION::CONSOLE, "console" },
	}
};

void controls::set(const action_event& evt, const controls::ACTION& action, const bool primary) {
	// check if the event is mapped to another event; if so, remove it
	const ACTION prev_action = get(evt);
	if(prev_action != ACTION::NONE) {
		auto& prev_action_entry = mapping.at(prev_action);
		if(prev_action_entry.first == evt) {
			prev_action_entry.first = 0;
		}
		else prev_action_entry.second = 0;
	}
	
	//
	const auto iter = mapping.find(action);
	if(iter == mapping.end()) {
		mapping.insert({ action, primary ? make_pair(evt, 0ULL) : make_pair(0ULL, evt) });
		return;
	}
	if(primary) iter->second.first = evt;
	else iter->second.second = evt;
}

void controls::clear(const controls::ACTION& action) {
	mapping.erase(action);
}

controls::ACTION controls::get(const action_event& evt) {
	for(const auto& entry : mapping) {
		if(entry.second.first == evt ||
		   entry.second.second == evt) {
			return entry.first;
		}
	}
	return ACTION::NONE;
}

pair<controls::action_event, controls::action_event> controls::get(const controls::ACTION& action) {
	const auto iter = mapping.find(action);
	if(iter == mapping.end()) {
		return { 0, 0 };
	}
	return iter->second;
}

string controls::name(const ACTION& action) {
	static const array<string, (unsigned int)ACTION::__MAX_ACTION> names {
		{
			"None",
			
			"PhysicsGun Pull",
			"PhysicsGun Push",
			"PhysicsGun Gravity",
			
			"Move Forwards",
			"Move Backwards",
			"Move Left",
			"Move Right",
			
			"Jump",
			
			"Console",
		}
	};
	if(action < ACTION::__MAX_ACTION) return names[(unsigned int)action];
	return names[0];
}

string controls::name(const action_event& evt) {
	if(evt == 0) return "";
	else if(evt <= 0xFFFFFFFFULL) {
		// key event
		const char* key_name = SDL_GetKeyName(((unsigned int)(evt & 0xFFFFFFFFULL)));
		return key_name;
	}
	switch(evt) {
		// mouse event
		case (unsigned long long int)MOUSE_EVENT_TYPE::LEFT: return "Left Mouse";
		case (unsigned long long int)MOUSE_EVENT_TYPE::RIGHT: return "Right Mouse";
		case (unsigned long long int)MOUSE_EVENT_TYPE::MIDDLE: return "Middle Mouse";
		default: break;
	}
	return "<unknown>";
}

pair<controls::ACTION, bool> controls::action_from_event(const EVENT_TYPE& type, const shared_ptr<event_object>& obj) {
	controls::action_event action_evt = 0;
	bool state = false;
	if((type & EVENT_TYPE::__KEY_EVENT) == EVENT_TYPE::__KEY_EVENT) {
		const key_event<EVENT_TYPE::__KEY_EVENT>& key_evt = (const key_event<EVENT_TYPE::__KEY_EVENT>&)*obj;
		action_evt = key_evt.key;
		state = (type == EVENT_TYPE::KEY_DOWN);
	}
	else if((type & EVENT_TYPE::__MOUSE_EVENT) == EVENT_TYPE::__MOUSE_EVENT) {
		switch(type) {
			case EVENT_TYPE::MOUSE_LEFT_DOWN: action_evt = (controls::action_event)controls::MOUSE_EVENT_TYPE::LEFT; state = true; break;
			case EVENT_TYPE::MOUSE_LEFT_UP: action_evt = (controls::action_event)controls::MOUSE_EVENT_TYPE::LEFT; state = false; break;
			case EVENT_TYPE::MOUSE_RIGHT_DOWN: action_evt = (controls::action_event)controls::MOUSE_EVENT_TYPE::RIGHT; state = true; break;
			case EVENT_TYPE::MOUSE_RIGHT_UP: action_evt = (controls::action_event)controls::MOUSE_EVENT_TYPE::RIGHT; state = false; break;
			case EVENT_TYPE::MOUSE_MIDDLE_DOWN: action_evt = (controls::action_event)controls::MOUSE_EVENT_TYPE::MIDDLE; state = true; break;
			case EVENT_TYPE::MOUSE_MIDDLE_UP: action_evt = (controls::action_event)controls::MOUSE_EVENT_TYPE::MIDDLE; state = false; break;
			default: return { controls::ACTION::NONE, false };
		}
	}
	else return { controls::ACTION::NONE, false };
	return { controls::get(action_evt), state };
}

void controls::load() {
	const xml::xml_doc& config_doc = e->get_config_doc();
	mapping.clear();
	
	for(const auto& action : conf_mapping) {
		const auto tokens = core::tokenize(config_doc.get<string>("config.bim.controls."+action.second, "0,0"), ',');
		if(tokens.size() != 2) {
			a2e_error("invalid token count (%u) for \"%s\"!", tokens.size(), action.second);
			continue;
		}
		set(string2ull(tokens[0]), action.first, true);
		set(string2ull(tokens[1]), action.first, false);
	}
}

void controls::save() {
	xml::xml_doc& config_doc = e->get_config_doc();
	
	for(const auto& action : conf_mapping) {
		const auto iter = mapping.find(action.first);
		if(iter == mapping.end()) continue;
		
		const string packed_event = ull2string(iter->second.first) + "," + ull2string(iter->second.second);
		config_doc.set<string>("config.bim.controls."+action.second, packed_event);
	}
}
