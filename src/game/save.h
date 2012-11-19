/*
 *  Blocks In Motion
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

#ifndef __SB_SAVE_GAME_H__
#define __SB_SAVE_GAME_H__

#include "sb_global.h"
#include "sb_map.h"

class save_game
{
public:
	static const size_t number_highscore_entries = 10;

	struct game_entry
	{
		string player_name = "<unknown>";
		string map_name = "intro.map";

		uint32_t level_progress = 0;
		uint32_t death_count = 0;
		uint32_t jump_count = 0;
		uint32_t step_count = 0;
		uint32_t passed_time = 0;
		uint64_t timestamp = 0;

		game_entry();

		string str() const;
		string save_string() const;
		string stats_string() const;

		bool operator<(const game_entry &entry) const;
		bool operator>(const game_entry &entry) const;
	};

	typedef std::vector<game_entry> game_entry_list;
	typedef game_entry_list::const_iterator iterator;

	save_game();

	const game_entry& current() const;

	iterator begin() const;
	iterator end() const;

	iterator begin_highscore() const;
	iterator end_highscore() const;

	void save();
	void dump_to_file() const;
	void load_game(const game_entry& entry);

	void start_game(const string& player_name);
	void set_map(const std::string &map_name);
	void inc_death_count();
	void inc_jump_count();
	void inc_step_count();
	
private:
	static string get_save_filename();
	void read_from_file();
	void recompute_highscore();

	uint32_t ticks_;

	game_entry_list highscore_;
	game_entry_list saves_;
	game_entry current_;
};

#endif
