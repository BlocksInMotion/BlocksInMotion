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

#ifndef __SB_MAP_RENDERER_H__
#define __SB_MAP_RENDERER_H__

#include "sb_global.h"
#include "sb_map.h"
#include <scene/model/a2estatic.h>
#include <scene/scene.h>
#include <gui/gui.h>
#include <rendering/gfx2d.h>

class block_textures;
class particle_system;
class map_renderer : protected a2estatic {
public:
	map_renderer();
	virtual ~map_renderer();
	
	virtual void draw(const DRAW_MODE draw_mode);
	
	void set_culling(const bool& state);
	
	a2e_texture get_particle_texture(const string& name);
	
protected:
	event::handler map_event_handler_fctr;
	bool map_event_handler(EVENT_TYPE type, shared_ptr<event_object> obj);
	
	GLuint draw_culling_vbo = 0;
	
	a2estatic* push_button = nullptr;
	array<a2ematerial*, 2> push_button_mat { { nullptr, nullptr } }; // off, on
	
	a2estatic* weight_trigger = nullptr;
	array<a2ematerial*, 2> weight_trigger_mat { { nullptr, nullptr } }; // off, on
	
	a2estatic* light_trigger = nullptr;
	array<a2ematerial*, 2> light_trigger_mat { { nullptr, nullptr } }; // off, on
	
	// particle systems
	vector<particle_system*> particles;
	unordered_map<sb_map::map_link*, particle_system*> ml_particles;
	unordered_map<string, a2e_texture> particle_textures;
	
	bool culling = true;
	
};

#endif
