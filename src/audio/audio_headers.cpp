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

#include "audio_headers.h"

LPALGENEFFECTS alGenEffects;
LPALDELETEEFFECTS alDeleteEffects;
LPALISEFFECT alIsEffect;
LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots;
LPALEFFECTI alEffecti;
LPALEFFECTF alEffectf;
LPALGENFILTERS alGenFilters;
LPALISFILTER alIsFilter;
LPALFILTERI alFilteri;
LPALFILTERF alFilterf;
LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti;
LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots;
LPALDELETEFILTERS alDeleteFilters;

bool init_efx_funcs() {
	alGenEffects = (LPALGENEFFECTS)alGetProcAddress("alGenEffects");
	alDeleteEffects = (LPALDELETEEFFECTS)alGetProcAddress("alDeleteEffects");
	alIsEffect = (LPALISEFFECT)alGetProcAddress("alIsEffect");
	alGenAuxiliaryEffectSlots = (LPALGENAUXILIARYEFFECTSLOTS)alGetProcAddress("alGenAuxiliaryEffectSlots");
	alEffecti = (LPALEFFECTI)alGetProcAddress("alEffecti");
	alEffectf = (LPALEFFECTF)alGetProcAddress("alEffectf");
	alGenFilters = (LPALGENFILTERS)alGetProcAddress("alGenFilters");
	alIsFilter = (LPALISFILTER)alGetProcAddress("alIsFilter");
	alFilteri = (LPALFILTERI)alGetProcAddress("alFilteri");
	alFilterf = (LPALFILTERF)alGetProcAddress("alFilterf");
	alAuxiliaryEffectSloti = (LPALAUXILIARYEFFECTSLOTI)alGetProcAddress("alAuxiliaryEffectSloti");
	alDeleteAuxiliaryEffectSlots = (LPALDELETEAUXILIARYEFFECTSLOTS)alGetProcAddress("alDeleteAuxiliaryEffectSlots");
	alDeleteFilters = (LPALDELETEFILTERS)alGetProcAddress("alDeleteFilters");
	
	const auto fail = [](const string& name) -> bool {
		a2e_error("failed to get function pointer for \"%s\"!", name);
		return false;
	};
	
	if(alGenEffects == nullptr) return fail("alGenEffects");
	if(alDeleteEffects == nullptr) return fail("alDeleteEffects");
	if(alIsEffect == nullptr) return fail("alIsEffect");
	if(alGenAuxiliaryEffectSlots == nullptr) return fail("alGenAuxiliaryEffectSlots");
	if(alEffecti == nullptr) return fail("alEffecti");
	if(alEffectf == nullptr) return fail("alEffectf");
	if(alGenFilters == nullptr) return fail("alGenFilters");
	if(alIsFilter == nullptr) return fail("alIsFilter");
	if(alFilteri == nullptr) return fail("alFilteri");
	if(alFilterf == nullptr) return fail("alFilterf");
	if(alAuxiliaryEffectSloti == nullptr) return fail("alAuxiliaryEffectSloti");
	if(alDeleteAuxiliaryEffectSlots == nullptr) return fail("alDeleteAuxiliaryEffectSlots");
	if(alDeleteFilters == nullptr) return fail("alDeleteFilters");
	
	return true;
}
