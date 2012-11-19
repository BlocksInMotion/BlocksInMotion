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

#include "save.h"
#include <engine.h>
#include <ctime>
#include <iomanip>

// gcc workaround ...
#if !defined(__clang__) && defined(__GNUC__)
string put_time(const std::tm* tmb, const char* fmt) {
	char buffer[256];
	memset(&buffer[0], 0, 256);
	strftime(buffer, 255, fmt, tmb);
	return buffer;
}
#endif

// game_entry

static void resolve_date(uint64_t milisec, uint32_t &hours, uint32_t &minutes, uint32_t &seconds)
{
	seconds = (uint32_t)(milisec / 1000ULL);
	minutes = seconds / 60;
	hours = minutes / 60;
	minutes -= hours * 60;
	seconds -= minutes * 60;
}

save_game::game_entry::game_entry() {
}

string save_game::game_entry::str() const
{
	uint32_t hours, minutes, seconds;
	resolve_date(passed_time, hours, minutes, seconds);
	
	//
	string map_title = "";
	file_io file(e->data_path("maps/"+map_name), file_io::OPEN_TYPE::READ_BINARY);
	if(file.is_open()) {
		file.get_uint(); // magic
		file.get_uint(); // version
		file.get_terminated_block(map_title, 0);
		file.close();
	}
	
	stringstream str;
	str << player_name;
	str << " \"";
	str << map_title;
	str << "\" [passed time: ";
	tm passed_timestamp;
	passed_timestamp.tm_hour = hours;
	passed_timestamp.tm_min = minutes;
	passed_timestamp.tm_sec = seconds;
	str << put_time(&passed_timestamp,
#if !defined(__clang__) && defined(__GNUC__)
					"%X"
#else
					"%EX"
#endif
					);
	str << "]";
	return str.str();
}

string save_game::game_entry::save_string() const {
	stringstream str;
	str << this->str();
	str << " [saved: ";
	str << put_time(std::localtime((time_t*)&timestamp),
#if !defined(__clang__) && defined(__GNUC__)
					"%x %X"
#else
					"%Ex %EX"
#endif
					);
	str << "]" << endl;
	return str.str();
}

string save_game::game_entry::stats_string() const
{
	stringstream str;
	str << this->str();
	str << " :: Level #" << level_progress;
	str << ", " << death_count << " deaths";
	str << ", " << jump_count << " jumps";
	str << ", " << step_count << " steps";
	return str.str();
}

bool save_game::game_entry::operator<(const game_entry &entry) const
{
	return (level_progress < entry.level_progress ||
			(passed_time < entry.passed_time ||
			 (death_count < entry.death_count ||
			  jump_count < entry.jump_count ||
			  step_count < entry.step_count)));
}

bool save_game::game_entry::operator>(const game_entry &entry) const
{
	return (level_progress > entry.level_progress ||
			(passed_time > entry.passed_time ||
			 (death_count > entry.death_count ||
			  jump_count > entry.jump_count ||
			  step_count > entry.step_count)));
}

static void dump_entry(FILE* file, const save_game::game_entry& entry)
{
	fprintf(file, "%u,%u,%s,%s,%u,%u,%u,%u,%u,%llu\n",
			(uint32_t)(entry.player_name.length() + 1), (uint32_t)(entry.map_name.length() + 1),
			entry.player_name.c_str(), entry.map_name.c_str(),
			entry.death_count, entry.jump_count, entry.step_count,
			entry.passed_time, entry.level_progress, entry.timestamp);
}

static void read_entry(FILE* file, save_game::game_entry& entry)
{
	uint32_t len1, len2;
	fscanf(file, "%u,%u,", &len1, &len2);
	char* buf = new char[std::max(len1, len2)];
	memset(buf, 0, len1);
	fgets(buf, len1, file);
	entry.player_name = string(buf);
	fscanf(file, ",");
	memset(buf, 0, len2);
	fgets(buf, len2, file);
	entry.map_name = string(buf);
	fscanf(file, ",%u,%u,%u,%u,%u,%llu\n",
		   &entry.death_count, &entry.jump_count, &entry.step_count,
		   &entry.passed_time, &entry.level_progress, &entry.timestamp);
	delete buf;
}

// save_game

save_game::save_game()
{
	read_from_file();
	start_game("<unknown>");
}

const save_game::game_entry& save_game::current() const
{
	return current_;
}

save_game::iterator save_game::begin() const
{
	return saves_.begin();
}

save_game::iterator save_game::end() const
{
	return saves_.end();
}

save_game::iterator save_game::begin_highscore() const
{
	return highscore_.begin();
}

save_game::iterator save_game::end_highscore() const
{
	return highscore_.end();
}

void save_game::save()
{
	// sets passed time
	current_.passed_time += SDL_GetTicks() - ticks_;
	current_.timestamp = (uint64_t)time(nullptr);
	// store to saves list
	saves_.push_back(current_);
	highscore_.push_back(current_);
	recompute_highscore();
}

void save_game::load_game(const game_entry& entry)
{
	start_game(entry.player_name);
	current_ = entry;
}

void save_game::start_game(const string& player_name_)
{
	ticks_ = SDL_GetTicks();
	current_ = game_entry();
	current_.player_name = player_name_;
}

void save_game::set_map(const std::string &map_name)
{
	if(current_.map_name != map_name) {
		++current_.level_progress;
		current_.map_name = map_name;
	}
}

void save_game::inc_death_count()
{
	++current_.death_count;
}

void save_game::inc_jump_count()
{
	++current_.jump_count;
}

void save_game::inc_step_count()
{
	++current_.step_count;
}

// internal functions

string save_game::get_save_filename()
{
	const string save_file("save.dat");
	return e->data_path(save_file);
}

void save_game::dump_to_file() const
{
	string save_file = get_save_filename();
	remove(save_file.c_str());
	FILE* file = fopen(save_file.c_str(), "wb");
	// dump highscore + saves
	fprintf(file, "%u,%u\n",
			(uint32_t)highscore_.size(), (uint32_t)saves_.size());
	for(const game_entry& ref : highscore_) {
		dump_entry(file, ref);
	}
	for(const game_entry& ref : saves_) {
		dump_entry(file, ref);
	}
	fclose(file);
}

void save_game::read_from_file()
{
	highscore_.clear();
	saves_.clear();
	string save_file = get_save_filename();
	FILE* file = fopen(save_file.c_str(), "rb");
	if(file == nullptr) return;
	uint32_t number_hs, number_sv;
	fscanf(file, "%u,%u\n", &number_hs, &number_sv);
	highscore_.reserve(number_hs);
	saves_.reserve(number_sv);
	for(size_t i = 0; i < number_hs; ++i) {
		game_entry entry;
		read_entry(file, entry);
		highscore_.push_back(entry);
	}
	for(size_t i = 0; i < number_sv; ++i) {
		game_entry entry;
		read_entry(file, entry);
		saves_.push_back(entry);
	}
	fclose(file);
	recompute_highscore();
}

void save_game::recompute_highscore()
{
	sort(highscore_.begin(), highscore_.end(), greater<game_entry>());
	while(highscore_.size() > number_highscore_entries) {
		highscore_.pop_back();
	}
}

