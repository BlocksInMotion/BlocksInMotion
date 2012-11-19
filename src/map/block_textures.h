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

#ifndef __SB_BLOCK_TEXTURES_H__
#define __SB_BLOCK_TEXTURES_H__

#include "sb_global.h"
#include <scene/model/a2ematerial.h>

enum class BLOCK_MATERIAL : unsigned int;
class block_textures {
public:
	block_textures();
	~block_textures();

	GLuint tex_id(const size_t& array_num) const;
	const a2e_texture& mat_tex(const unsigned int& mat_index, const size_t& sub_type) const;
	
protected:
	a2ematerial material;
	// diffuse, specular, reflectance, normal, height, aux (iso/aniso)
	static constexpr size_t texture_count = 6;
	GLuint tex_arrays[texture_count] = { 0, 0, 0, 0, 0 };

};

#endif
