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

#ifndef __SB_GLOBAL_H__
#define __SB_GLOBAL_H__

#include <core/cpp_headers.h>
#include <core/logger.h>
#include <core/util.h>
#include <core/vector2.h>
#include <core/vector3.h>
#include <core/vector4.h>
#include <core/matrix4.h>
#include <core/core.h>
#include "sb_events.h"
#include "sb_conf.h"
#include "controls.h"

#define APPLICATION_NAME "Blocks In Motion"
#define APPLICATION_VERSION ""
#define APPLICATION_TITLE APPLICATION_NAME " " APPLICATION_VERSION

#define APPLICATION_VERSION_STRING (string(APPLICATION_NAME)+" "+APPLICATION_VERSION+" "+ \
A2E_PLATFORM+A2E_DEBUG_STR+" ("+A2E_BUILD_DATE+" "+A2E_BUILD_TIME+ \
") built with "+string(A2E_COMPILER+A2E_LIBCXX))

// engine:
class engine;
class file_io;
class gfx;
class texman;
class event;
class shader;
class opencl_base;
class scene;
class camera;
class ext;
class gui;
class font_manager;
class particle_manager;

extern engine* e;
extern file_io* fio;
extern texman* t;
extern event* eevt;
extern shader* s;
extern opencl_base* ocl;
extern scene* sce;
extern camera* cam;
extern ext* exts;
extern gui* ui;
extern font_manager* fm;
extern particle_manager* pm;

// application:
class block_textures;
class audio_controller;
class physics_controller;
class sb_map;
class physics_player;
class audio_store;
class editor;
class game;
class script_handler;
class map_renderer;
class menu_ui;
class save_game;
class sb_console;
extern block_textures* bt;
extern audio_controller* ac;
extern physics_controller* pc;
extern sb_map* active_map;
extern physics_player* player;
extern audio_store* as;
extern editor* ed;
extern game* ge;
extern script_handler* sh;
extern map_renderer* mr;
extern menu_ui* menu;
extern save_game* save;
extern sb_console* console;

struct mpg123_handle_struct;
typedef struct mpg123_handle_struct mpg123_handle;
extern mpg123_handle* mh;

#endif
