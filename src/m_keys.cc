//------------------------------------------------------------------------
//  KEY BINDINGS
//------------------------------------------------------------------------
//
//  Eureka DOOM Editor
//
//  Copyright (C) 2013 Andrew Apted
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

#include "main.h"


typedef struct
{
	const char *name;
	command_func_t func;

} editor_command_t;


static std::vector<editor_command_t *> all_commands;


/* this should only be called during startup */
void M_RegisterCommand(const char *name, command_func_t func)
{
	editor_command_t *cmd = new editor_command_t;

	cmd->name = name;
	cmd->func = func;

	all_commands.push_back(cmd);
}


static const editor_command_t * FindEditorCommand(const char *name)
{
	for (unsigned int i = 0 ; i < all_commands.size() ; i++)
		if (y_stricmp(all_commands[i]->name, name) == 0)
			return all_commands[i];

	return NULL;
}


//------------------------------------------------------------------------


// FIXME: TABLE OF KEY NAMES <--> CODES


/* returns zero (an invalid key) if parsing fails */
keycode_t M_ParseKeyString(const char *str)
{
	int key = 0;

	if (y_strnicmp(str, "CMD-", 4) == 0)
	{
		key |= MOD_COMMAND;  str += 4;
	}
	else if (y_strnicmp(str, "META-", 5) == 0)
	{
		key |= MOD_META;  str += 5;
	}
	else if (y_strnicmp(str, "ALT-", 4) == 0)
	{
		key |= MOD_ALT;  str += 4;
	}
	else if (y_strnicmp(str, "SHIFT-", 6) == 0)
	{
		key |= MOD_META;  str += 6;
	}

	if (str[0] > 32 && str[0] < 127 && isprint(str[0]))
		return key | (unsigned char) str[0];

	if (str[0] == '0' && str[1] == 'x')
		return key | atoi(str);

	// FIXME: FIND NAME IN TABLE

	return 0;
}


static const char * BareKeyName(keycode_t key)
{
	static char buffer[200];

	if (key < 127 && key > 32 && isprint(key))
	{
		buffer[0] = (char) key;
		buffer[1] = 0;

		return buffer;
	}

	// FIXME: FIND KEY IN TABLE

	// fallback : hex code

	sprintf(buffer, "0x%04x", key);

	return buffer;
}


const char * M_KeyToString(keycode_t key)
{
	const char *mod = "";

	if (key & MOD_COMMAND)
		mod = "CMD-";
	else if (key & MOD_META)
		mod = "META-";
	else if (key & MOD_ALT)
		mod = "ALT-";
	else if (key & MOD_SHIFT)
		mod = "SHIFT-";


	static char buffer[200];

	strcpy(buffer, mod);

	strcat(buffer, BareKeyName(key & FL_KEY_MASK));

	return buffer;
}


//------------------------------------------------------------------------


typedef enum
{
	KCTX_INVALID = 0,

	KCTX_Global,
	KCTX_Browser,
	KCTX_Render,

	KCTX_Line,
	KCTX_Sector,
	KCTX_Thing,
	KCTX_Vertex,
	KCTX_RadTrig,

	KCTX_Edit

} key_context_e;


int M_ParseKeyContext(const char *str)
{
	if (y_stricmp(str, "global")  == 0) return KCTX_Global;
	if (y_stricmp(str, "browser") == 0) return KCTX_Browser;
	if (y_stricmp(str, "render")  == 0) return KCTX_Render;
	if (y_stricmp(str, "line")    == 0) return KCTX_Line;
	if (y_stricmp(str, "sector")  == 0) return KCTX_Sector;
	if (y_stricmp(str, "thing")   == 0) return KCTX_Thing;
	if (y_stricmp(str, "vertex")  == 0) return KCTX_Vertex;
	if (y_stricmp(str, "radtrig") == 0) return KCTX_RadTrig;
	if (y_stricmp(str, "edit")    == 0) return KCTX_Edit;

	return KCTX_INVALID;
}

const char * M_KeyContextString(key_context_e context)
{
	switch (context)
	{
		case KCTX_Global:  return "global";
		case KCTX_Browser: return "browser";
		case KCTX_Render:  return "render";
		case KCTX_Line:    return "line";
		case KCTX_Sector:  return "sector";
		case KCTX_Thing:   return "thing";
		case KCTX_Vertex:  return "vertex";
		case KCTX_RadTrig: return "radtrig";
		case KCTX_Edit:    return "edit";

		default:
			break;
	}

	return "INVALID";
}


#define MAX_BIND_PARAM_LEN  16

typedef struct
{
	keycode_t key;

	key_context_e context;

	const editor_command_t *cmd;

	char param1[MAX_BIND_PARAM_LEN];
	char param2[MAX_BIND_PARAM_LEN];

} key_binding_t;


static std::vector<key_binding_t> all_bindings;


void M_ParseBindings()
{
	all_bindings.clear();

	// TODO
}


void M_WriteBindings()
{
	static char filename[FL_PATH_MAX];

	sprintf(filename, "%s/bindings.cfg", home_dir);

	FILE *fp = fopen(filename, "w");

	if (! fp)
	{
		LogPrintf("Failed to save key bindings to: %s\n", filename);
		return;
	}

	LogPrintf("Writing key bindings to: %s\n", filename);

	fprintf(fp, "# Eureka key bindings\n");
	fprintf(fp, "# vi:ts=16:noexpandtab\n\n");

	for (unsigned int i = 0 ; i < all_bindings.size() ; i++)
	{
		key_binding_t& bind = all_bindings[i];

		if (bind.context == KCTX_INVALID)
			continue;
		
		fprintf(fp, "%s\t%s\t%s", M_KeyContextString(bind.context),
		        M_KeyToString(bind.key), bind.cmd->name);

		if (bind.param1[0]) fprintf(fp, "\t%s", bind.param1);
		if (bind.param2[0]) fprintf(fp, "\t%s", bind.param2);

		fprintf(fp, "\n");
	}
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
