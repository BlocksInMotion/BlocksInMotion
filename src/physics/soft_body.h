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

#ifndef __SB_SOFT_BODY_H__
#define __SB_SOFT_BODY_H__

#include <BulletDynamics/btBulletDynamicsCommon.h>
#include "sb_global.h"
#include <scene/model/a2estatic.h>

struct soft_info {
	const float mass;
	const bool pose_volume;
	const bool pose_bframe;
	
	// taken from btSoftBody::Config:
	const float kVCF; // Velocities correction factor (Baumgarte)
	const float kDP; // Damping coefficient [0,1]
	const float kDG; // Drag coefficient [0,+inf]
	const float kLF; // Lift coefficient [0,+inf]
	const float kPR; // Pressure coefficient [-inf,+inf]
	const float kVC; // Volume conversation coefficient [0,+inf]
	const float kDF; // Dynamic friction coefficient [0,1]
	const float kMT; // Pose matching coefficient [0,1]
	const float kCHR; // Rigid contacts hardness [0,1]
	const float kKHR; // Kinetic contacts hardness [0,1]
	const float kSHR; // Soft contacts hardness [0,1]
	const float kAHR; // Anchors hardness [0,1]
};

class btSoftBody;
class soft_body : public a2estatic {
public:
	soft_body(btSoftBodyWorldInfo& world_info, const string& filename, const float3& position, const soft_info& sinfo);
	virtual ~soft_body();
	
	void update_model();
	
	btSoftBody* get_body();
	const float3& get_avg_position() const;
	const soft_info& get_construction_info() const;
	
	virtual void draw(const DRAW_MODE draw_mode);
	
protected:
	soft_info construction_info;
	btSoftBody* body;
	float3 avg_position;

};

#endif
