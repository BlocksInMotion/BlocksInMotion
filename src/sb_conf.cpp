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

#include "sb_conf.h"
#include "sb_global.h"
#include "audio_controller.h"
#include <engine.h>
#include <rendering/texture_object.h>
#include <core/xml.h>

//
unordered_map<string, pair<conf::CONF_TYPE, void*>> conf::settings;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// define add, get and set functions for all valid conf types

//// get
#if !defined(DEBUG)
// non-debug version
#define CONF_DEFINE_GET_FUNCS(type, enum_type) \
template <> void conf::get<type>(const string& name, type& dst) { dst = ((setting<type>*)(settings[name].second))->get(); } \
template <> const type& conf::get<type>(const string& name) { return ((setting<type>*)(settings[name].second))->get(); }

#else
// debug version with run-time type checking
#define CONF_DEFINE_GET_FUNCS(type, enum_type) \
template <> void conf::get<type>(const string& name, type& dst) {													\
	if(((setting<type>*)(settings[name].second))->type_name != typeid(type).name()) {								\
		a2e_error("invalid type for conf setting \"%s\" - used: %s, expected: %s!",									\
				  name, typeid(type).name(), ((setting<type>*)(settings[name].second))->type_name);					\
	}																												\
	dst = ((setting<type>*)(settings[name].second))->get();															\
}																													\
\
template <> const type& conf::get<type>(const string& name) {														\
	if(((setting<type>*)(settings[name].second))->type_name != typeid(type).name()) {								\
		a2e_error("invalid type for conf setting \"%s\" - used: %s, expected: %s!",									\
				  name, typeid(type).name(), ((setting<type>*)(settings[name].second))->type_name);					\
	}																												\
	return ((setting<type>*)(settings[name].second))->get();														\
}

#endif

SB_CONF_TYPES(CONF_DEFINE_GET_FUNCS)

//// set
#if !defined(DEBUG)
// non-debug version
#define CONF_DEFINE_SET_FUNCS(type, enum_type) \
template <> void conf::set<type>(const string& name, const type& value) {											\
	((setting<type>*)(settings[name].second))->set(value);															\
}
#else
// debug version with run-time type checking
#define CONF_DEFINE_SET_FUNCS(type, enum_type) \
template <> void conf::set<type>(const string& name, const type& value) {											\
	if(((setting<type>*)(settings[name].second))->type_name != typeid(type).name()) {								\
		a2e_error("invalid type for conf setting \"%s\" - used: %s, expected: %s!",									\
				  name, typeid(type).name(), ((setting<type>*)(settings[name].second))->type_name);					\
	}																												\
	((setting<type>*)(settings[name].second))->set(value);															\
}
#endif

SB_CONF_TYPES(CONF_DEFINE_SET_FUNCS)

//// add
#define CONF_DEFINE_ADD_FUNCS(type, enum_type) \
template <> bool conf::add<type>(const string& name, const type& value,												\
								 decltype(setting<type>::call_on_set) call_on_set) {								\
	/* check if a setting with such a name already exists */														\
	if(settings.count(name) > 0) {																					\
		a2e_error("a setting with the name \"%s\" already exists!", name);											\
		return false;																								\
	}																												\
																													\
	/* create setting */																							\
	settings.insert(make_pair(name, make_pair(enum_type, (void*)setting<type>::create(value, call_on_set))));		\
	return true;																									\
}																													\
template <> bool conf::add<type>(const string& name, const type& value) {											\
	return conf::add<type>(name, value, [](const type& val a2e_unused){});											\
}
SB_CONF_TYPES(CONF_DEFINE_ADD_FUNCS)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

void conf::init() {
	const xml::xml_doc& config_doc = e->get_config_doc();
	controls::load();
	
	// graphics settings:
	// ultra, high (default), mid, low
	conf::add<string>("gfx.texture_quality", config_doc.get<string>("config.bim.gfx.texture.quality", "high"));
	conf::add<bool>("gfx.particles", config_doc.get<bool>("config.bim.gfx.particles.enabled", true));
	
	// audio settings:
	conf::add<float>("volume.music", config_doc.get<float>("config.bim.volume.music", 1.0f), [](const float& val a2e_unused) {
		ac->reset_volume();
	});
	conf::add<float>("volume.sound", config_doc.get<float>("config.bim.volume.sound", 1.0f), [](const float& val a2e_unused) {
		ac->reset_volume();
	});
	
	// visualization settings:
	conf::add<float4>("nvis_grab_color", config_doc.get<float4>("config.bim.forcefield.grab_color", float4(0.0f, 0.0f, 1.0f, 1.0f)));
	conf::add<float4>("nvis_push_color", config_doc.get<float4>("config.bim.forcefield.push_color", float4(1.0f, 0.0f, 0.0f, 1.0f)));
	conf::add<float4>("nvis_swap_color", config_doc.get<float4>("config.bim.forcefield.nvis_swap_color", float4(0.0f, 1.0f, 0.0f, 1.0f)));
	conf::add<float4>("nvis_bg_selecting", config_doc.get<float4>("config.bim.forcefield.background_selecting", float4(0.0f, 0.0f, 0.0f, 0.0f)));
	conf::add<float4>("nvis_bg_selected", config_doc.get<float4>("config.bim.forcefield.background_selected", float4(0.0f, 0.0f, 0.0f, 1.0f)));
	conf::add<float2>("nvis_line_interval", config_doc.get<float2>("config.bim.forcefield.line_interval", float2(0.1f, 0.13f)));
	conf::add<float>("nvis_tex_interpolation", config_doc.get<float>("config.bim.forcefield.tex_interpolation", 0.4f));
	conf::add<float>("nvis_time_denominator", config_doc.get<float>("config.bim.forcefield.time_denominator", 100000.0f));
	
	// for misc debugging purposes:
	conf::add<bool>("debug.ui", false);
	conf::add<size_t>("debug.texture", 0); // actual GLuint
	conf::add<bool>("debug.show_texture", false, [](const bool& val) {
		conf::set<bool>("debug.ui", val);
	});
	conf::add<bool>("debug.fps", false, [](const bool& val) {
		conf::set<bool>("debug.ui", val);
	});
	conf::add<bool>("debug.osx", false, [](const bool& val) {
		conf::set<bool>("debug.ui", val);
	});
	conf::add<bool>("debug.timer", false, [](const bool& val) {
		conf::set<bool>("debug.ui", val);
	});
	conf::add<bool>("debug.no_audio", false);
	conf::add<bool>("debug.no_menu", config_doc.get<bool>("config.bim.menu.disabled", true));
}
