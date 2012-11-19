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

#ifndef __SB_BUILTIN_MODELS_H__
#define __SB_BUILTIN_MODELS_H__

#include "sb_global.h"

class builtin_models {
public:
	builtin_models() = delete;
	~builtin_models() = delete;
	
	static void init();
	
	static const size_t cube_vertex_count;
	static const size_t cube_index_count;
	static const vector<float3> cube_vertices;
	static const vector<uchar3> cube_indices;
	static const vector<coord> cube_tex_coords;
	static vector<float3> cube_normals;
	static vector<float3> cube_binormals;
	static vector<float3> cube_tangents;
	static const vector<unsigned int> cube_culling;
	
};

#endif
