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

#ifndef __SB_CONSOLE_H__
#define __SB_CONSOLE_H__

#include "sb_global.h"
#include <gui/gui.h>
#include <scene/scene.h>

class font;
class sb_console {
public:
	sb_console();
	~sb_console();
	
	void set_enabled(const bool state);
	bool is_enabled() const;
	
	void handle();
	
protected:
	bool enabled = false; // don't change this here
	font* fnt = nullptr;
	
	void add_line(const string& line_str, const bool add_input_character = true);
	static constexpr size_t display_line_count = 12;
	static constexpr float margin = 10.0f;
	deque<pair<string, pair<uint2, float2>>> display_lines;
	vector<string> user_lines;
	vector<unsigned int> user_input;
	ssize_t selected_input_line = -1; // -1 -> last
	ssize_t selected_input_char = -1; // -1 -> last
	
	ui_draw_callback draw_cb;
	gui_simple_callback* ui_cb_obj = nullptr;
	void draw_ui(const DRAW_MODE_UI draw_mode, rtt::fbo* buffer);
	
	event::handler key_handler_fnctr;
	bool key_handler(EVENT_TYPE type, shared_ptr<event_object> obj);
	
	void execute_cmd(const string& cmd_str);
	enum class COMMAND : unsigned int {
		INVALID,
		SET,
		GET,
		LIST,
		HELP,
		STATS,
		LOAD,
		SAVE,
		EDITOR,
		RUN,
	};
	static const unordered_map<string, COMMAND> commands;
	
	deque<string> command_queue;

};

#endif
