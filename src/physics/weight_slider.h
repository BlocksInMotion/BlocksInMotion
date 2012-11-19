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

#ifndef __SB_WEIGHT_SLIDER_H__
#define __SB_WEIGHT_SLIDER_H__

#include "sb_global.h"
#include <BulletDynamics/btBulletDynamicsCommon.h>

class a2emodel;
class rigid_body;
struct rigid_info;
class weight_slider {
public:
	weight_slider(const float3& position, const float& height, const float& mass);
	~weight_slider();
	
	rigid_body* get_base_body();
	rigid_body* get_tray_body();
	btSliderConstraint* get_constraint();
	
	float get_height() const;
	float get_normalized_height() const;
	float get_render_height() const;
	
	bool is_triggered() const;
	
	void update();
	float get_avg_normalized_height() const;
	
protected:
	const float3 position;
	const float height;
	const float mass;

	rigid_body* base_body;
	rigid_body* tray_body;
	btSliderConstraint* constraint;
	
	rigid_info* base_info;
	rigid_info* tray_info;
	
	array<float, 32> avg_norm_height;
	size_t cur_avg_pos = 0;
	
};

#endif
