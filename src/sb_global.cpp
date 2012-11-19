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

#include "sb_global.h"
#include <a2e.h>

// engine:
engine* e = nullptr;
file_io* fio = nullptr;
texman* t = nullptr;
event* eevt = nullptr;
shader* s = nullptr;
opencl_base* ocl = nullptr;
scene* sce = nullptr;
camera* cam = nullptr;
ext* exts = nullptr;
gui* ui = nullptr;
font_manager* fm = nullptr;
particle_manager* pm = nullptr;

// application:
block_textures* bt = nullptr;
audio_controller* ac = nullptr;
physics_controller* pc = nullptr;
mpg123_handle* mh = nullptr;
sb_map* active_map = nullptr;
physics_player* player = nullptr;
audio_store* as = nullptr;
editor* ed = nullptr;
game* ge = nullptr;
script_handler* sh = nullptr;
map_renderer* mr = nullptr;
menu_ui* menu = nullptr;
save_game* save = nullptr;
sb_console* console = nullptr;
