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

#ifndef __SB_MAIN_H__
#define __SB_MAIN_H__

#include "sb_global.h"
#include "sb_conf.h"
#include "map/sb_map.h"
#include "map/map_storage.h"
#include "map/map_renderer.h"
#include "map/block_textures.h"
#include "map/builtin_models.h"
#include "editor/editor.h"
#include "game/game.h"
#include "game/save.h"
#include "ui/sb_console.h"
#include "audio/audio_controller.h"
#include "physics/physics_controller.h"
#include "physics/rigid_body.h"
#include "physics/soft_body.h"
#include "physics/physics_player.h"
#include "ai/ai_entity.h"
#include "scripting/script.h"
#include "scripting/script_handler.h"
#include "sb_debug.h"
#include "ui/menu_ui.h"
#include <a2e.h>
#include <gui/font.h>
#include <gui/font_manager.h>
#include <rendering/gl_timer.h>

// prototypes
bool event_handler(EVENT_TYPE type, shared_ptr<event_object> obj);

#endif
