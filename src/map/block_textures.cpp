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

#include "block_textures.h"
#include "sb_map.h"

#if !defined(__APPLE__)
// ..., internal format, format, type, bpp, red shift, green shift, blue shift, alpha shift
#define __TEXTURE_FORMATS(F, src_surface, dst_format) \
F(src_surface, dst_format, GL_R8, GL_RED, GL_UNSIGNED_BYTE, 8, 0, 0, 0, 0) \
F(src_surface, dst_format, GL_RG8, GL_RG, GL_UNSIGNED_BYTE, 16, 0, 8, 0, 0) \
F(src_surface, dst_format, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, 24, 0, 8, 16, 0) \
F(src_surface, dst_format, GL_RGB8, GL_RGBA, GL_UNSIGNED_BYTE, 32, 0, 8, 16, 0) \
F(src_surface, dst_format, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 32, 0, 8, 16, 24) \
F(src_surface, dst_format, GL_BGR8, GL_BGR, GL_UNSIGNED_BYTE, 24, 16, 8, 0, 0) \
F(src_surface, dst_format, GL_BGR8, GL_BGRA, GL_UNSIGNED_BYTE, 32, 16, 8, 0, 0) \
F(src_surface, dst_format, GL_BGRA8, GL_BGRA, GL_UNSIGNED_BYTE, 32, 16, 8, 0, 24) \
F(src_surface, dst_format, GL_R16, GL_RED, GL_UNSIGNED_SHORT, 16, 0, 0, 0, 0) \
F(src_surface, dst_format, GL_RG16, GL_RG, GL_UNSIGNED_SHORT, 32, 0, 16, 0, 0) \
F(src_surface, dst_format, GL_RGB16, GL_RGB, GL_UNSIGNED_SHORT, 48, 0, 16, 32, 0) \
F(src_surface, dst_format, GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT, 64, 0, 16, 32, 48)

#define __CHECK_FORMAT(src_surface, dst_format, \
					   gl_internal_format, gl_format, gl_type, bpp, rshift, gshift, bshift, ashift) \
if(src_surface->format->Rshift == rshift && \
   src_surface->format->Gshift == gshift && \
   src_surface->format->Bshift == bshift && \
   src_surface->format->Ashift == ashift && \
   src_surface->format->BitsPerPixel == bpp) { \
	dst_format = gl_format; \
}

#define check_format(surface, format) { \
	__TEXTURE_FORMATS(__CHECK_FORMAT, surface, format); \
}
#endif

constexpr size_t block_textures::texture_count;

block_textures::block_textures() : material(::e) {
	int2 tex_size(1024, 1024);
	if(conf::get<string>("gfx.texture_quality") == "ultra") {
		material.load_material(e->data_path("level_ultra.a2mtl"));
		a2e_debug("loading ultra-quality textures ...");
	}
	else if(conf::get<string>("gfx.texture_quality") == "high") {
		tex_size.set(512, 512);
		material.load_material(e->data_path("level_high.a2mtl"));
		a2e_debug("loading high-quality textures ...");
	}
	else if(conf::get<string>("gfx.texture_quality") == "mid") {
		tex_size.set(256, 256);
		material.load_material(e->data_path("level_mid.a2mtl"));
		a2e_debug("loading mid-quality textures ...");
	}
	else {
		tex_size.set(128, 128);
		material.load_material(e->data_path("level_low.a2mtl"));
		a2e_debug("loading low-quality textures ...");
	}
	
	// create tex arrays
	glGenTextures(texture_count, &tex_arrays[0]);
	const auto filtering = e->get_filtering();
	
#if !defined(__APPLE__)
	if(exts->get_vendor() == ext::GRAPHICS_CARD_VENDOR::ATI) {
		const size_t size_per_texture = tex_size.x * tex_size.y * 4;
		unsigned char* textures = new unsigned char[size_per_texture * material.get_material_count()];
		
		for(size_t i = 0; i < texture_count; i++) {
			glBindTexture(GL_TEXTURE_2D_ARRAY, tex_arrays[i]);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, (filtering == TEXTURE_FILTERING::POINT ? GL_NEAREST : GL_LINEAR));
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, texman::select_filter(filtering));
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
			if(filtering >= TEXTURE_FILTERING::BILINEAR && e->get_anisotropic() > 0) {
				glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY_EXT, (GLint)e->get_anisotropic());
			}
			
			for(unsigned int zoffset = 0; zoffset < (material.get_material_count() - 1); zoffset++) {
				const a2e_texture& tex = mat_tex(zoffset + 1, i);
				SDL_Surface* tex_surface = IMG_Load(tex->filename.c_str());
				if(tex_surface == nullptr) {
					a2e_error("error loading texture file \"%s\": %s!", tex->filename, SDL_GetError());
					return;
				}
				
				// figure out the textures format
				GLenum format = 0;
				check_format(tex_surface, format);
				
				// if the format is not RGBA, convert it
				if(format != GL_RGBA) {
					SDL_PixelFormat new_pformat;
					memcpy(&new_pformat, tex_surface->format, sizeof(SDL_PixelFormat));
					
					new_pformat.Ashift = 24;
					new_pformat.Bshift = 16;
					new_pformat.Gshift = 8;
					new_pformat.Rshift = 0;
					
					new_pformat.Amask = 0xFF000000;
					new_pformat.Bmask = 0xFF0000;
					new_pformat.Gmask = 0xFF00;
					new_pformat.Rmask = 0xFF;
					
					new_pformat.BytesPerPixel = 4;
					new_pformat.BitsPerPixel = 32;
					
					SDL_Surface* new_surface = SDL_ConvertSurface(tex_surface, &new_pformat, 0);
					if(new_surface == nullptr) {
						a2e_error("surface conversion failed for %s!", tex->filename);
						return;
					}
					else {
						SDL_FreeSurface(tex_surface);
						tex_surface = new_surface;
					}
				}
				
				memcpy(&textures[size_per_texture * zoffset], tex_surface->pixels, size_per_texture);
				SDL_FreeSurface(tex_surface);
			}
			
			glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, tex_size.x, tex_size.y,
						 (GLsizei)material.get_material_count(),
						 0, GL_RGBA, GL_UNSIGNED_BYTE, textures);
			
			if(filtering >= TEXTURE_FILTERING::BILINEAR) glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
		}
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
		
		delete [] textures;
	}
	else {
#endif
		// create tmp fbo for copying
		GLuint fbo = 0;
		glGenFramebuffers(1, &fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		
		static const pair<GLint, GLenum> types[texture_count] = {
			make_pair(GL_RGBA8, GL_RGBA),
			make_pair(GL_RGB8, GL_RGB),
			make_pair(GL_RGB8, GL_RGB),
			make_pair(GL_RGB8, GL_RGB),
			make_pair(GL_R8, GL_RED),
			make_pair(GL_RG8, GL_RG),
		};
		
		for(size_t i = 0; i < texture_count; i++) {
			glBindTexture(GL_TEXTURE_2D_ARRAY, tex_arrays[i]);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, (filtering == TEXTURE_FILTERING::POINT ? GL_NEAREST : GL_LINEAR));
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, texman::select_filter(filtering));
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
			if(filtering >= TEXTURE_FILTERING::BILINEAR && e->get_anisotropic() > 0) {
				glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY_EXT, (GLint)e->get_anisotropic());
			}
			
			glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, types[i].first, tex_size.x, tex_size.y,
						 (GLsizei)material.get_material_count(),
						 0, types[i].second, GL_UNSIGNED_BYTE, nullptr);
			
			// copy textures to array
			for(unsigned int zoffset = 0; zoffset < (material.get_material_count() - 1); zoffset++) {
				GLuint tex = mat_tex(zoffset + 1, i)->tex();
				
				// on-gpu copy ftw
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
				glCopyTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, (GLint)zoffset, 0, 0, tex_size.x, tex_size.y);
			}
			
			if(filtering >= TEXTURE_FILTERING::BILINEAR) glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
		}
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &fbo);
		
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
#if !defined(__APPLE__)
	}
#endif
}

block_textures::~block_textures() {
	glDeleteTextures(texture_count, &tex_arrays[0]);
}

GLuint block_textures::tex_id(const size_t& array_num) const {
	return tex_arrays[array_num];
}

const a2e_texture& block_textures::mat_tex(const unsigned int& mat_index, const size_t& sub_type) const {
	const a2ematerial::material& default_mat = material.get_material(0); // black and white texture
	const a2e_texture& black_tex = ((a2ematerial::diffuse_material*)default_mat.mat)->diffuse_texture;
	if(sub_type >= texture_count) return black_tex;
	
	const a2ematerial::material& mat = material.get_material(mat_index);
	if(sub_type <= 4) { // 0, 1, 2, 3, 4
		switch(mat.mat_type) {
			case a2ematerial::MATERIAL_TYPE::DIFFUSE: {
				a2ematerial::diffuse_material* dmat = (a2ematerial::diffuse_material*)mat.mat;
				if(sub_type == 0) return dmat->diffuse_texture;
				if(sub_type == 1) return dmat->specular_texture;
				if(sub_type == 2) return dmat->reflectance_texture;
			}
			break;
			case a2ematerial::MATERIAL_TYPE::PARALLAX: {
				a2ematerial::parallax_material* pmat = (a2ematerial::parallax_material*)mat.mat;
				if(sub_type == 0) return pmat->diffuse_texture;
				if(sub_type == 1) return pmat->specular_texture;
				if(sub_type == 2) return pmat->reflectance_texture;
				if(sub_type == 3) return pmat->normal_texture;
				if(sub_type == 4) return pmat->height_texture;
			}
			break;
			default: break;
		}
	}
	else { // 5
		switch(mat.lm_type) {
			case a2ematerial::LIGHTING_MODEL::ASHIKHMIN_SHIRLEY: {
				a2ematerial::ashikhmin_shirley_model* asmodel = (a2ematerial::ashikhmin_shirley_model*)mat.model;
				if(sub_type == 5) {
					if(asmodel->anisotropic_texture != nullptr) {
						return asmodel->anisotropic_texture;
					}
				}
			}
			break;
			default: break;
		}
	}
	
	return black_tex;
}
