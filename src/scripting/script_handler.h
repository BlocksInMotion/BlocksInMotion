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

#ifndef __SB_SCRIPT_HANDLER_H__
#define __SB_SCRIPT_HANDLER_H__

#include "sb_global.h"

class script;
class script_handler {
public:
	script_handler();
	~script_handler();
	
	script* load_script(const string& filename);
	script* reload_script(const string& filename);
	script* get_script(const string& filename) const;
	
protected:
	// <filename, script object>
	unordered_map<string, script*> scripts;

};

#endif
