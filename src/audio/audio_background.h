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

#ifndef __SB_AUDIO_BACKGROUND_H__
#define __SB_AUDIO_BACKGROUND_H__

#include "sb_global.h"
#include "audio_source.h"

class audio_background : public audio_source {
public:
	audio_background(const audio_store::audio_data& data, const string& identifier);
	virtual ~audio_background();

	virtual void set_volume(const float& volume);
	virtual float get_volume() const;

};

#endif
