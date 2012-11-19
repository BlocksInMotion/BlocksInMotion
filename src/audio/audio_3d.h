/*
 *  Blocks In Motion
 *  Copyright (C) 2012 Yannic Haupenthal
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

#ifndef __SB_AUDIO_3D_H__
#define __SB_AUDIO_3D_H__

#include "sb_global.h"
#include "audio_source.h"

enum class AUDIO_EFFECT : unsigned int;
class audio_3d : public audio_source {
public:
	audio_3d(const audio_store::audio_data& data, const string& identifier);
	virtual ~audio_3d();
	
	const float3& get_position() const;
	void set_position(const float3& pos);
	const float3& get_velocity() const;
	void set_velocity(const float3& vel);
	
	float get_reference_distance() const;
	void set_reference_distance(const float ref_distance);
	float get_rolloff_factor() const;
	void set_rolloff_factor(const float rolloff_factor);
	float get_max_distance() const;
	void set_max_distance(const float max_distance);

	virtual void set_volume(const float& volume);
	virtual float get_volume() const;
	
	virtual bool is_playing() const;
	virtual bool is_initial() const;

protected:
	array<ALuint, 2> ui_effect { { 0,0 } };
	array<ALuint, 1> ui_filter { { 0 } };
	
	void set_effect(const AUDIO_EFFECT& effect) const;
	void unset_effect() const;

};

#endif
