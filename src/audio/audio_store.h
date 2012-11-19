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

#ifndef __SB_AUDIO_STORE_H__
#define __SB_AUDIO_STORE_H__

#include "sb_global.h"
#include "audio_headers.h"

// these are the only effects supported by all openal implementations
enum class AUDIO_EFFECT : unsigned int {
	NONE,
	REVERB,
	ECHO
};

class audio_store {
public:
	audio_store();
	~audio_store();
	
	static constexpr float default_volume = 1.0f;
	static const float3 default_velocity;
	static constexpr float default_reference_distance = 5.0f;
	static constexpr float default_rolloff_factor = 3.5f;
	static constexpr float default_max_distance = 1000.0f;
	
	struct audio_data {
		const string filename;
		const ALuint buffer;
		const ALenum format;
		const ALsizei freq;
		const float3 velocity;
		const float volume;
		const float reference_distance;
		const float rolloff_factor;
		const float max_distance;
		const vector<AUDIO_EFFECT> effects;
	};
	
	audio_data* load_file(const string& filename, const string& identifier, const vector<AUDIO_EFFECT>& effects = vector<AUDIO_EFFECT> {});
	
	bool has_audio_data(const string& identifier) const;
	const audio_data& get_audio_data(const string& identifier) const;
	
	const unordered_map<string, audio_data*>& get_store() const;
	
protected:
	// <identifier, data>
	unordered_map<string, audio_data*> store;
	
};

#endif
