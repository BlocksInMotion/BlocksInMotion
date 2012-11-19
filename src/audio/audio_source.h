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

#ifndef __SB_AUDIO_SOURCE_H__
#define __SB_AUDIO_SOURCE_H__

#include "sb_global.h"
#include "audio_store.h"

class audio_source {
public:
	audio_source(const audio_store::audio_data& data, const string& identifier);
	virtual ~audio_source();
	
	virtual void play();
	virtual void stop();
	virtual void halt();
	virtual void loop(const bool state = true);

	virtual void set_volume(const float& volume) = 0;
	virtual float get_volume() const = 0;
	
	bool set_identifier(const string& identifier);
	const string& get_identifier() const;

	virtual bool is_playing() const;
	virtual bool is_looping() const;

	virtual void set_play_on_load(const bool play_on_load);
	virtual bool get_play_on_load() const;
	
	virtual void set_can_be_killed(const bool can_be_killed);
	virtual bool get_can_be_killed() const;

protected:
	string identifier;
	ALuint source = 0;
	float3 position;
	float3 velocity;
	float3 orientation;
	float ref_distance = 0.0f;
	float rolloff_factor = 0.0f;
	float max_distance = 0.0f;
	float volume = 0.0f;
	bool playing = false;
	bool looping = false;
	bool play_on_load = false;
	bool can_be_killed = true;

};

#endif
