//------------------------------------------------------------------------
//  CONFIG FILE
//------------------------------------------------------------------------
//
//  Eureka DOOM Editor
//
//  Copyright (C) 2001-2015 Andrew Apted
//  Copyright (C) 1997-2003 Andr� Majorel et al
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
//
//  Based on Yadex which incorporated code from DEU 5.21 that was put
//  in the public domain in 1994 by Rapha�l Quinet and Brendon Wyber.
//
//------------------------------------------------------------------------

#include "main.h"

#include <sys/types.h>
#include <sys/stat.h>

#include "lib_adler.h"
#include "editloop.h"
#include "e_loadsave.h"
#include "im_color.h"
#include "m_config.h"
#include "r_grid.h"
#include "r_render.h"
#include "levels.h"

#include "ui_window.h"  // meh!



//------------------------------------------------------------------------

/*
 *  Description of the command line arguments and config file keywords
 */
typedef enum
{
	// End of the options description
	OPT_END = 0,

	// Boolean (toggle)
	// Receptacle is of type: bool
	OPT_BOOLEAN,

	// Integer number,
	// Receptacle is of type: int
	OPT_INTEGER,

	// A color value
	// Receptacle is of type: rgb_color_t
	OPT_COLOR,

	// String
	// Receptacle is of type: const char *
	OPT_STRING,

	// List of strings
	// Receptacle is of type: std::vector< const char * >
	OPT_STRING_LIST
}
opt_type_t;


typedef struct
{
	const char *long_name;  // Command line arg. or keyword
	const char *short_name; // Abbreviated command line argument

	opt_type_t opt_type;    // Type of this option
	const char *flags;    // Flags for this option :
	// '1' : process only on pass 1 of parse_command_line_options()
	// 'f' : string is a filename
	// '<' : print extra newline after this option (when dumping)
	// 'v' : a real variable (preference setting)
	// 'w' : warp hack -- accept two numeric args

	const char *desc;   // Description of the option
	const char *arg_desc;  // Description of the argument (NULL --> none or default)

	void *data_ptr;   // Pointer to the data
}
opt_desc_t;


static const opt_desc_t options[] =
{
	//
	// A few options must be handled in an early pass
	//

	{	"help",
		"h",
		OPT_BOOLEAN,
		"1",
		"Show usage summary",
		NULL,
		&show_help
	},

	{	"version",
		"v",
		OPT_BOOLEAN,
		"1",
		"Show version info",
		NULL,
		&show_version
	},

	{	"debug",
		"d",
		OPT_BOOLEAN,
		"1",
		"Enable debugging messages",
		NULL,
		&Debugging
	},

	{	"quiet",
		"q",
		OPT_BOOLEAN,
		"1<",
		"Quiet mode (no messages on stdout)",
		NULL,
		&Quiet
	},


	{	"home",
		0,
		OPT_STRING,
		"1",
		"Home directory",
		"<dir>",
		&home_dir
	},

	{	"install",
		0,
		OPT_STRING,
		"1",
		"Installation directory",
		"<dir>",
		&install_dir
	},

	{	"log",
		0,
		OPT_STRING,
		"1",
		"Log messages to specified file",
		"<file>",
		&log_file
	},

	{	"config",
		0,
		OPT_STRING,
		"1<",
		"Config file to load / save",
		"<file>",
		&config_file
	},

	//
	// Normal options from here on....
	//

	{	"iwad",
		"i",
		OPT_STRING,
		"",
		"The name of the IWAD (game data)",
		"<file>",
		&Iwad_name
	},

	{	"file",
		"f",
		OPT_STRING_LIST,
		"",
		"Patch wad file(s) to edit",
		"<file>...",
		&Pwad_list
	},

	{	"merge",
		"m",
		OPT_STRING_LIST,
		"",
		"Resource file(s) to load",
		"<file>...",
		&Resource_list
	},

	{	"port",
		"p",
		OPT_STRING,
		"",
		"Port (engine) name",
		"<name>",
		&Port_name
	},

	{	"warp",
		"w",
		OPT_STRING,
		"w",
		"Select level to edit",
		"<map>",
		&Level_name
	},

	/* ------------ Preferences ------------ */

	{	"auto_load_recent",
		0,
		OPT_BOOLEAN,
		"v",
		"When no given files, load the most recent one saved",
		NULL,
		&auto_load_recent
	},

	{	"begin_maximized",
		0,
		OPT_BOOLEAN,
		"v",
		"Maximize the window when Eureka starts",
		NULL,
		&begin_maximized
	},

	{	"backup_max_files",
		0,
		OPT_INTEGER,
		"v",
		"Maximum copies to make when backing up a wad",
		NULL,
		&backup_max_files
	},

	{	"backup_max_space",
		0,
		OPT_INTEGER,
		"v",
		"Maximum space to use (in MB) when backing up a wad",
		NULL,
		&backup_max_space
	},

	{	"default_grid_mode",
		0,
		OPT_INTEGER,
		"v",
		"Default grid mode: 0 = OFF, 1 = dotty, 2 = normal",
		NULL,
		&default_grid_mode
	},

	{	"default_grid_size",
		0,
		OPT_INTEGER,
		"v",
		"Default grid size",
		NULL,
		&default_grid_size
	},

	{	"default_grid_snap",
		0,
		OPT_BOOLEAN,
		"v",
		"Default grid snapping",
		NULL,
		&default_grid_snap
	},

	{	"default_edit_mode",
		0,
		OPT_INTEGER,
		"v",
		"Default editing mode: 0..3 = Th / Lin / Sec / Vt",
		NULL,
		&default_edit_mode
	},

	{	"default_port",
		0,
		OPT_STRING,
		"v",
		"Default port (engine) name",
		NULL,
		&default_port
	},

	{	"digits_set_zoom",
		0,
		OPT_BOOLEAN,
		"v",
		"Digit keys set zoom factor (rather than grid size)",
		NULL,
		&digits_set_zoom
	},

	{	"dotty_axis_col",
		0,
		OPT_COLOR,
		"v",
		"axis color for the dotty style grid",
		NULL,
		&dotty_axis_col
	},

	{	"dotty_major_col",
		0,
		OPT_COLOR,
		"v",
		"major color for the dotty style grid",
		NULL,
		&dotty_major_col
	},

	{	"dotty_minor_col",
		0,
		OPT_COLOR,
		"v",
		"minor color for the dotty style grid",
		NULL,
		&dotty_minor_col
	},

	{	"dotty_point_col",
		0,
		OPT_COLOR,
		"v",
		"point color for the dotty style grid",
		NULL,
		&dotty_point_col
	},

	{	"glbsp_fast",
		0,
		OPT_BOOLEAN,
		"v",
		"Node building: enable fast mode (may be lower quality)",
		NULL,
		&glbsp_fast
	},

	{	"glbsp_verbose",
		0,
		OPT_BOOLEAN,
		"v",
		"Node building: be verbose, show level information",
		NULL,
		&glbsp_verbose
	},

	{	"glbsp_warn",
		0,
		OPT_BOOLEAN,
		"v",
		"Node building: show all warning messages",
		NULL,
		&glbsp_warn
	},

	{	"grid_hide_in_free_mode",
		0,
		OPT_BOOLEAN,
		"v",
		"hide the grid in FREE mode",
		NULL,
		&grid_hide_in_free_mode
	},

	{	"grid_toggle_type",
		0,
		OPT_INTEGER,
		"v",
		"grid toggle type : 0 = BOTH, 1 = dotty, 2 = normal",
		NULL,
		&grid_toggle_type
	},

	{	"gui_scheme",
		0,
		OPT_INTEGER,
		"v",
		"GUI widget theme: 0 = fltk, 1 = gtk+, 2 = plastic",
		NULL,
		&gui_scheme
	},

	{	"gui_color_set",
		0,
		OPT_INTEGER,
		"v",
		"GUI color set: 0 = fltk default, 1 = bright, 2 = custom",
		NULL,
		&gui_color_set
	},

	{	"gui_custom_bg",
		0,
		OPT_COLOR,
		"v",
		"GUI custom background color",
		NULL,
		&gui_custom_bg
	},

	{	"gui_custom_ig",
		0,
		OPT_COLOR,
		"v",
		"GUI custom input color",
		NULL,
		&gui_custom_ig
	},

	{	"gui_custom_fg",
		0,
		OPT_COLOR,
		"v",
		"GUI custom foreground (text) color",
		NULL,
		&gui_custom_fg
	},

	{	"leave_offsets_alone",
		0,
		OPT_BOOLEAN,
		"v",
		"Do not adjust offsets when splitting lines (etc)",
		NULL,
		&leave_offsets_alone
	},

	{	"map_scroll_bars",
		0,
		OPT_BOOLEAN,
		"v",
		"Enable scroll-bars for the map view",
		NULL,
		&map_scroll_bars
	},

	{	"mouse_wheel_scrolls_map",
		0,
		OPT_BOOLEAN,
		"v",
		"Use the mouse wheel events for scrolling, not zooming",
		NULL,
		&mouse_wheel_scrolls_map
	},

	{	"multi_select_modifier",
		0,
		OPT_INTEGER,
		"v",
		"Require a modifier key for multi-select (0 = none, 1 = SHIFT, 2 = CTRL)",
		NULL,
		&multi_select_modifier
	},

	{	"new_islands_are_void",
		0,
		OPT_BOOLEAN,
		"v",
		"Islands created inside a sector will get a void interior",
		NULL,
		&new_islands_are_void
	},

	{	"new_sector_size",
		0,
		OPT_INTEGER,
		"v",
		"Size of sector rectangles created outside of the map",
		NULL,
		&new_sector_size
	},

	{	"normal_axis_col",
		0,
		OPT_COLOR,
		"v",
		"axis color for the normal grid",
		NULL,
		&normal_axis_col
	},

	{	"normal_main_col",
		0,
		OPT_COLOR,
		"v",
		"main color for the normal grid",
		NULL,
		&normal_main_col
	},

	{	"normal_flat_col",
		0,
		OPT_COLOR,
		"v",
		"flat color for the normal grid",
		NULL,
		&normal_flat_col
	},

	{	"normal_small_col",
		0,
		OPT_COLOR,
		"v",
		"small color for the normal grid",
		NULL,
		&normal_small_col
	},

	{	"render_aspect_ratio",
		0,
		OPT_INTEGER,
		"v",
		"Aspect ratio for 3D view : 100 * width / height",
		NULL,
		&render_aspect_ratio
	},

	{	"render_lock_gravity",
		0,
		OPT_BOOLEAN,
		"v",
		"Locked gravity in 3D view -- cannot move up or down",
		NULL,
		&render_lock_gravity
	},

	{	"render_missing_bright",
		0,
		OPT_BOOLEAN,
		"v",
		"Render the missing texture as fullbright",
		NULL,
		&render_missing_bright
	},

	{	"render_unknown_bright",
		0,
		OPT_BOOLEAN,
		"v",
		"Render the unknown texture as fullbright",
		NULL,
		&render_unknown_bright
	},

	{	"same_mode_clears_selection",
		0,
		OPT_BOOLEAN,
		"v",
		"Clear the selection when entering the same mode",
		NULL,
		&same_mode_clears_selection
	},

	{	"scroll_less",
		0,
		OPT_INTEGER,
		"v",
		"Amp. of scrolling (% of screen size)",
		NULL,
		&scroll_less
	},

	{	"scroll_more",
		0,
		OPT_INTEGER,
		"v",
		"Amp. of scrolling (% of screen size)",
		NULL,
		&scroll_more
	},

	{	"swap_sidedefs",
		0,
		OPT_BOOLEAN,
		"v",
		"Swap upper and lower sidedefs in the Linedef panel",
		NULL,
		&swap_sidedefs
	},

// -AJA- this may be better done with a 'mouse_button_order' string,
//       where the default is "123", and user could have "321"
//       or "132" or any other combination.
#if 0
	{	"swap_buttons",
		0,
		OPT_BOOLEAN,
		"v",
		"Swap mouse buttons",
		NULL,
		&swap_buttons
	},
#endif

	//
	// That's all there is
	//

	{	0,
		0,
		OPT_END,
		0,
		0,
		0,
		0
	}
};


//------------------------------------------------------------------------

static int parse_config_line_from_file(char *p, const char *basename, int lnum)
{
	char *name  = NULL;
	char *value = NULL;

	// skip leading whitespace
	while (isspace (*p))
		p++;

	// skip comments
	if (*p == '#')
		return 0;

	// remove trailing newline and whitespace
	{
		int len = (int)strlen(p);

		while (len > 0 && isspace(p[len - 1]))
		{
			p[len - 1] = 0;
			len--;
		}
	}

	// skip empty lines
	if (*p == 0)
		return 0;

	// grab the name
	name = p;
	while (y_isident (*p))
		p++;

	if (! isspace(*p))
	{
		LogPrintf("WARNING: %s(%u): bad line, no space after keyword.\n", basename, lnum);
		return 0;
	}

	*p++ = 0;

	// find the option value (occupies rest of the line)
	while (isspace(*p))
		p++;

	if (*p == 0)
	{
		LogPrintf("WARNING: %s(%u): bad line, missing option value.\n", basename, lnum);
		return 0;
	}

	value = p;

	// find the option keyword
	const opt_desc_t * opt;

	for (opt = options ; ; opt++)
	{
		if (opt->opt_type == OPT_END)
		{
			LogPrintf("WARNING: %s(%u): invalid option '%s', skipping\n",
					  basename, lnum, name);
			return 0;
		}

		if (! opt->long_name || strcmp(name, opt->long_name) != 0)
			continue;

		// pre-pass options (like --help) don't make sense in a config file
		if (strchr(opt->flags, '1'))
		{
			LogPrintf("WARNING: %s(%u): cannot use option '%s' in config files.\n",
			          basename, lnum, name);
			return 0;
		}

		// found it
		break;
	}

	switch (opt->opt_type)
	{
		case OPT_BOOLEAN:
			if (y_stricmp(value, "no")    == 0 ||
				y_stricmp(value, "false") == 0 ||
				y_stricmp(value, "off")   == 0 ||
				y_stricmp(value, "0")     == 0)
			{
				*((bool *) (opt->data_ptr)) = false;
			}
			else  // anything else is TRUE
			{
				*((bool *) (opt->data_ptr)) = true;
			}
			break;

		case OPT_INTEGER:
			*((int *) opt->data_ptr) = atoi(value);
			break;

		case OPT_COLOR:
			*((rgb_color_t *) opt->data_ptr) = ParseColor(value);
			break;

		case OPT_STRING:
			// handle empty string
			if (strcmp(value, "''") == 0)
				*value = 0;

			*((char **) opt->data_ptr) = StringDup(value);
			break;

		case OPT_STRING_LIST:
			while (*value != 0)
			{
				char *v = value;
				while (*v != 0 && ! isspace ((unsigned char) *v))
					v++;

				string_list_t * list = (string_list_t *)opt->data_ptr;

				list->push_back(StringDup(value, (int)(v - value)));

				while (isspace (*v))
					v++;
				value = v;
			}
			break;

		default:
			BugError("INTERNAL ERROR: unknown option type %d", (int) opt->opt_type);
			return -1;
	}

	return 0;  // OK
}


/*
 *  try to parse a config file by pathname.
 *
 *  Return 0 on success, negative value on failure.
 */
static int parse_a_config_file(FILE *fp, const char *filename)
{
	static char line[1024];

	const char *basename = FindBaseName(filename);

	// Execute one line on each iteration
	for (int lnum = 1 ; fgets(line, sizeof(line), fp) != NULL ; lnum++)
	{
		int ret = parse_config_line_from_file(line, basename, lnum);

		if (ret != 0)
			return ret;
	}

	return 0;  // OK
}


static const char * default_config_file()
{
	static char filename[FL_PATH_MAX];

	SYS_ASSERT(home_dir);

	sprintf(filename, "%s/config.cfg", home_dir);

	return StringDup(filename);
}


/*
 *  parses the config file (either a user-specific one or the default one).
 *
 *  return 0 on success, negative value on error.
 */
int M_ParseConfigFile()
{
	if (! config_file)
	{
		config_file = default_config_file();
	}

	FILE * fp = fopen(config_file, "r");

	LogPrintf("Reading config file: %s\n", config_file);

	if (fp == NULL)
	{
		LogPrintf("--> %s\n", strerror(errno));
		return -1;
	}

	int rc = parse_a_config_file(fp, config_file);

	fclose(fp);

	return rc;
}


/*
 *  parse_environment_vars
 *  Check certain environment variables.
 *  Returns 0 on success, <>0 on error.
 */
int M_ParseEnvironmentVars()
{
#if 0
	char *value;

	value = getenv ("EUREKA_GAME");
	if (value != NULL)
		Game = value;
#endif

	return 0;
}


void M_AddPwadName(const char *filename)
{
	Pwad_list.push_back(StringDup(filename));
}


/*
 *  parses the command line options
 *
 *  If <pass> is set to 1, ignores all options except those
 *  that have the "1" flag.
 *  Else, ignores all options that have the "1" flag.
 *  If an error occurs, report it with LogPrintf().
 *  and returns non-zero. Else, returns 0.
 */
int M_ParseCommandLine(int argc, const char *const *argv, int pass)
{
	const opt_desc_t *o;

	while (argc > 0)
	{
		bool ignore;

		// is it actually an option?
		if (argv[0][0] != '-')
		{
			// this is a loose file, handle it now
			if (pass != 1)
				M_AddPwadName(argv[0]);

			argv++;
			argc--;
			continue;
		}

		// Which option is this?
		for (o = options; ; o++)
		{
			if (o->opt_type == OPT_END)
			{
				FatalError("unknown option: '%s'\n", argv[0]);
				return 1;
			}

			if ( (o->short_name && strcmp (argv[0]+1, o->short_name) == 0) ||
				 (o->long_name  && strcmp (argv[0]+1, o->long_name ) == 0) ||
				 (o->long_name  && argv[0][1] == '-' && strcmp (argv[0]+2, o->long_name ) == 0) )
				break;
		}

		// ignore options which are not meant for this pass
		ignore = (strchr(o->flags, '1') != NULL) != (pass == 1);

		switch (o->opt_type)
		{
			case OPT_BOOLEAN:
				// -AJA- permit a following value
				if (argc >= 2 && argv[1][0] != '-')
				{
					argv++;
					argc--;

					if (ignore)
						break;

					if (y_stricmp(argv[0], "no")    == 0 ||
					    y_stricmp(argv[0], "false") == 0 ||
						y_stricmp(argv[0], "off")   == 0 ||
						y_stricmp(argv[0], "0")     == 0)
					{
						*((bool *) (o->data_ptr)) = false;
					}
					else  // anything else is TRUE
					{
						*((bool *) (o->data_ptr)) = true;
					}
				}
				else if (! ignore)
				{
					*((bool *) o->data_ptr) = true;
				}
				break;

			case OPT_INTEGER:
				if (argc < 2)
				{
					FatalError("missing argument after '%s'\n", argv[0]);
					return 1;
				}

				argv++;
				argc--;

				if (! ignore)
				{
					*((int *) o->data_ptr) = atoi(argv[0]);
				}
				break;

			case OPT_COLOR:
				if (argc < 2)
				{
					FatalError("missing argument after '%s'\n", argv[0]);
					return 1;
				}

				argv++;
				argc--;

				if (! ignore)
				{
					*((rgb_color_t *) o->data_ptr) = ParseColor(argv[0]);
				}
				break;

			case OPT_STRING:
				if (argc < 2)
				{
					FatalError("missing argument after '%s'\n", argv[0]);
					return 1;
				}

				argv++;
				argc--;

				if (! ignore)
				{
					*((const char **) o->data_ptr) = StringDup(argv[0]);
				}

				// support two numeric values after -warp
				if (strchr(o->flags, 'w') && isdigit(argv[0][0]) &&
					argc > 1 && isdigit(argv[1][0]))
				{
					if (! ignore)
					{
						*((const char **) o->data_ptr) = StringPrintf("%s%s", argv[0], argv[1]);
					}

					argv++;
					argc--;
				}

				break;

			case OPT_STRING_LIST:
				if (argc < 2)
				{
					FatalError("missing argument after '%s'\n", argv[0]);
					return 1;
				}
				while (argc > 1 && argv[1][0] != '-' && argv[1][0] != '+')
				{
					argv++;
					argc--;

					if (! ignore)
					{
						string_list_t * list = (string_list_t *) o->data_ptr;
						list->push_back(StringDup(argv[0]));
					}
				}
				break;

			default:
				{
					BugError("INTERNAL ERROR: unknown option type (%d)", (int) o->opt_type);
					return 1;
				}
		}

		argv++;
		argc--;
	}
	return 0;
}


/*
 *  Print a list of the parameters with their current value.
 */
void dump_parameters(FILE *fp)
{
	const opt_desc_t *o;
	int desc_maxlen = 0;
	int name_maxlen = 0;

	for (o = options; o->opt_type != OPT_END; o++)
	{
		int len = (int)strlen (o->desc);
		desc_maxlen = MAX(desc_maxlen, len);
		if (o->long_name)
		{
			len = (int)strlen (o->long_name);
			name_maxlen = MAX(name_maxlen, len);
		}
	}

	for (o = options; o->opt_type != OPT_END; o++)
	{
		if (! o->long_name)
			continue;
		
		fprintf (fp, "%-*s  %-*s  ", name_maxlen, o->long_name, desc_maxlen, o->desc);

		if (o->opt_type == OPT_BOOLEAN)
			fprintf (fp, "%s", *((bool *) o->data_ptr) ? "true" : "false");
		else if (o->opt_type == OPT_INTEGER)
			fprintf (fp, "%d", *((int *) o->data_ptr));
		else if (o->opt_type == OPT_COLOR)
			fprintf (fp, "%06x", *((rgb_color_t *) o->data_ptr) >> 8);
		else if (o->opt_type == OPT_STRING)
		{
			const char *str = *((const char **) o->data_ptr);
			fprintf(fp, "'%s'", str ? str : "--none--");
		}
		else if (o->opt_type == OPT_STRING_LIST)
		{
			string_list_t *list = (string_list_t *)o->data_ptr;

			if (list->empty())
				fprintf(fp, "--none--");
			else for (unsigned int i = 0 ; i < list->size() ; i++)
				fprintf(fp, "'%s' ", list->at(i));
		}
		fputc ('\n', fp);
	}
}


/*
 *  Print a list of all command line options (usage message).
 */
void dump_command_line_options(FILE *fp)
{
	const opt_desc_t *o;
	int name_maxlen = 0;
	int  arg_maxlen = 0;

	for (o = options; o->opt_type != OPT_END; o++)
	{
		int len;

		if (strchr(o->flags, 'v'))
			continue;

		if (o->long_name)
		{
			len = (int)strlen (o->long_name);
			name_maxlen = MAX(name_maxlen, len);
		}

		if (o->arg_desc)
		{
			len = (int)strlen (o->arg_desc);
			arg_maxlen = MAX(arg_maxlen, len);
		}
	}

	for (o = options; o->opt_type != OPT_END; o++)
	{
		if (strchr(o->flags, 'v'))
			continue;

		if (o->short_name)
			fprintf (fp, "  -%-3s ", o->short_name);
		else
			fprintf (fp, "       ");

		if (o->long_name)
			fprintf (fp, "-%-*s   ", name_maxlen, o->long_name);
		else
			fprintf (fp, "%*s  ", name_maxlen + 2, "");

		if (o->arg_desc)
			fprintf (fp, "%-12s", o->arg_desc);
		else switch (o->opt_type)
		{
			case OPT_BOOLEAN:       fprintf (fp, "            "); break;
			case OPT_INTEGER:       fprintf (fp, "<value>     "); break;
			case OPT_COLOR:         fprintf (fp, "<color>     "); break;

			case OPT_STRING:        fprintf (fp, "<string>    "); break;
			case OPT_STRING_LIST:   fprintf (fp, "<string> ..."); break;
			case OPT_END: ;  // This line is here only to silence a GCC warning.
		}

		fprintf (fp, " %s\n", o->desc);

		if (strchr(o->flags, '<'))
			fprintf (fp, "\n");
	}
}


int M_WriteConfigFile()
{
	SYS_ASSERT(config_file);

	LogPrintf("Writing config file: %s\n", config_file);

	FILE * fp = fopen(config_file, "w");

	if (! fp)
	{
		LogPrintf("--> %s\n", strerror(errno));
		return -1;
	}

	const opt_desc_t *o;

	for (o = options; o->opt_type != OPT_END; o++)
	{
		if (! strchr(o->flags, 'v'))
			continue;

		if (! o->long_name)
			continue;

		fprintf(fp, "%s ", o->long_name);

		switch (o->opt_type)
		{
			case OPT_BOOLEAN:
				fprintf(fp, "%s", *((bool *) o->data_ptr) ? "1" : "0");
				break;

			case OPT_STRING:
			{
				const char *str = *((const char **) o->data_ptr);
				fprintf(fp, "%s", (str && str[0]) ? str : "''");
				break;
			}

			case OPT_INTEGER:
				fprintf(fp, "%d", *((int *) o->data_ptr));
				break;

			case OPT_COLOR:
				fprintf(fp, "%06x", *((rgb_color_t *) o->data_ptr) >> 8);
				break;

			case OPT_STRING_LIST:
			{
				string_list_t *list = (string_list_t *)o->data_ptr;

				if (list->empty())
					fprintf(fp, "{}");
				else for (unsigned int i = 0 ; i < list->size() ; i++)
					fprintf(fp, "%s ", list->at(i));
			}

			default:
				break;
		}

		fprintf(fp, "\n");
	}

	fflush(fp);
	fclose(fp);

	return 0;  // OK
}


//------------------------------------------------------------------------
//   USER STATE HANDLING
//------------------------------------------------------------------------


int M_ParseLine(const char *line, const char ** tokens, int max_tok, bool do_strings)
{
	int num_tok = 0;

	char tokenbuf[256];
	int  tokenlen = -1;

	bool in_string = false;

	// skip leading whitespace
	while (isspace(*line))
		line++;
	
	// blank line or comment line?
	if (*line == 0 || *line == '\n' || *line == '#')
		return 0;

	for (;;)
	{
		if (tokenlen > 250)  // ERROR: token too long
			return -1;

		int ch = *line++;

		if (tokenlen < 0)  // looking for a new token
		{
			SYS_ASSERT(!in_string);

			// end of line?  if yes, break out of for loop
			if (ch == 0 || ch == '\n')
				break;

			if (isspace(ch))
				continue;

			tokenlen = 0;

			// begin a string?
			if (ch == '"' && do_strings)
			{
				in_string = true;
				continue;
			}

			// begin a normal token
			tokenbuf[tokenlen++] = ch;
			continue;
		}

		if (ch == '"' && in_string)
		{
			// end of string
			in_string = false;
		}
		else if (ch == 0 || ch == '\n' || isspace(ch))
		{
			// end of token
		}
		else
		{
			tokenbuf[tokenlen++] = ch;
			continue;
		}

		if (num_tok >= max_tok)  // ERROR: too many tokens
			return -2;

		if (in_string)  // ERROR: non-terminated string
			return -3;

		tokenbuf[tokenlen] = 0;
		tokenlen = -1;

		tokens[num_tok++] = strdup(tokenbuf);

		// end of line?  if yes, break out of for loop
		if (ch == 0 || ch == '\n')
			break;
	}

	return num_tok;
}


void M_FreeLine(const char ** tokens, int num_tok)
{
	for (int i = 0 ; i < num_tok ; i++)
	{
		free((void *) tokens[i]);

		tokens[i] = NULL;
	}
}



char * PersistFilename(const crc32_c& crc)
{
	static char filename[FL_PATH_MAX];

	sprintf(filename, "%s/cache/%08X%08X.dat", cache_dir,
	        crc.extra, crc.raw);

	return filename;
}


#define MAX_TOKENS  10


bool M_LoadUserState()
{
	crc32_c crc;

	BA_LevelChecksum(crc);


	char *filename = PersistFilename(crc);

	LogPrintf("Load user state from: %s\n", filename);

	FILE *fp = fopen(filename, "r");

	if (! fp)
	{
		LogPrintf("--> %s\n", strerror(errno));
		return false;
	}

	static char line_buf[FL_PATH_MAX];

	const char * tokens[MAX_TOKENS];

	while (! feof(fp))
	{
		char *line = fgets(line_buf, FL_PATH_MAX, fp);

		if (! line)
			break;

		StringRemoveCRLF(line);

		int num_tok = M_ParseLine(line, tokens, MAX_TOKENS, true /* do_strings */);

		if (num_tok == 0)
			continue;

		if (num_tok < 0)
		{
			LogPrintf("Error in persistent data: %s\n", line);
			continue;
		}

		if (  Editor_ParseUser(tokens, num_tok) ||
		        Grid_ParseUser(tokens, num_tok) ||
		    Render3D_ParseUser(tokens, num_tok) ||
		     Browser_ParseUser(tokens, num_tok) ||
		       Props_ParseUser(tokens, num_tok) ||
		     RecUsed_ParseUser(tokens, num_tok))
		{
			// Ok
		}
		else
		{
			LogPrintf("Unknown persistent data: %s\n", line);
		}
	}

	fclose(fp);

	Props_LoadValues();

	return true;
}


bool M_SaveUserState()
{
	crc32_c crc;

	BA_LevelChecksum(crc);


	char *filename = PersistFilename(crc);

	LogPrintf("Save user state to: %s\n", filename);

	FILE *fp = fopen(filename, "w");

	if (! fp)
	{
		LogPrintf("--> FAILED! (%s)\n", strerror(errno));
		return false;
	}

	  Editor_WriteUser(fp);
	    Grid_WriteUser(fp);
	Render3D_WriteUser(fp);
	 Browser_WriteUser(fp);
	   Props_WriteUser(fp);
	 RecUsed_WriteUser(fp);

	fclose(fp);

	return true;
}


void M_DefaultUserState()
{
	grid.Init();

	CMD_ZoomWholeMap();

	Render3D_Setup();
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
