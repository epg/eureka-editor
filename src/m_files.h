//------------------------------------------------------------------------
//  RECENT FILES / KNOWN IWADS / BACKUPS
//------------------------------------------------------------------------
//
//  Eureka DOOM Editor
//
//  Copyright (C) 2012-2013 Andrew Apted
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#ifndef __EUREKA_M_FILES_H__
#define __EUREKA_M_FILES_H__

#include "m_strings.h"
#include "sys_type.h"

#include <deque>

class Wad_file;



//------------------------------------------------------------------------
//  RECENT FILE HANDLING
//------------------------------------------------------------------------

#define MAX_RECENT  24


// this is for the "File/Recent" menu callbacks
class recent_file_data_c
{
public:
	fs::path file;
	SString map;

public:
	recent_file_data_c(const SString &_file, const SString &_map) :
		file(fs::u8path(_file.get())), map(_map)
	{ }

	recent_file_data_c(const recent_file_data_c &other) = default;

	recent_file_data_c() = default;
};


class RecentFiles_c
{
private:
	typedef std::deque<recent_file_data_c> Deque;

	void push_front(const SString &file, const SString &map);
	Deque::iterator find(const SString &file);

	Deque list;

public:

	int getSize() const
	{
		return (int)list.size();
	}

	recent_file_data_c *getData(int index) const;
	void clear()
	{
		list.clear();
	}
	
	void insert(const SString &file, const SString &map);
	void WriteFile(FILE *fp) const;
	SString Format(int index) const;
	void Lookup(int index, SString *file_v, SString *map_v) const;
};


void M_LoadRecent();
void M_SaveRecent();

void M_AddRecent(const SString &filename, const SString &map_name);
void M_OpenRecentFromMenu(void *priv_data);

// these three only for menu code
int M_RecentCount();
SString M_RecentShortName(int index);
void * M_RecentData(int index);


void M_LookForIWADs();
void M_AddKnownIWAD(const SString &path);
SString M_QueryKnownIWAD(const SString &game);
SString M_CollectGamesForMenu(int *exist_val, const char *exist_name);

void M_ValidateGivenFiles();
int  M_FindGivenFile(const char *filename);

void M_BackupWad(Wad_file *wad);


struct port_path_info_t
{
	SString exe_filename;
};

port_path_info_t * M_QueryPortPath(const SString &name, bool create_it = false);

bool M_IsPortPathValid(const port_path_info_t *info);

bool readBuffer(FILE* f, size_t size, std::vector<byte>& target);

#endif  /* __EUREKA_M_FILES_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
