//------------------------------------------------------------------------
//  OBJECT OPERATIONS
//------------------------------------------------------------------------
//
//  Eureka DOOM Editor
//
//  Copyright (C) 2001-2016 Andrew Apted
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

#include <algorithm>

#include "e_linedef.h"
#include "e_sector.h"
#include "e_things.h"
#include "e_vertex.h"
#include "editloop.h"
#include "levels.h"
#include "objects.h"
#include "r_grid.h"
#include "w_rawdef.h"
#include "x_hover.h"
#include "x_loop.h"

#include "ui_window.h"


// config items
bool new_islands_are_void = false;
int  new_sector_size = 128;

bool easier_drawing_mode = true;


/*
   delete a group of objects.
   The selection is no longer valid after this!
*/
void DeleteObjects(selection_c *list)
{
	// we need to process the object numbers from highest to lowest,
	// because each deletion invalidates all higher-numbered refs
	// in the selection.  Our selection iterator cannot give us
	// what we need, hence put them into a vector for sorting.

	if (list->empty())
		return;

	std::vector<int> objnums;

	selection_iterator_c it;
	for (list->begin(&it) ; !it.at_end() ; ++it)
		objnums.push_back(*it);

	std::sort(objnums.begin(), objnums.end());

	for (int i = (int)objnums.size()-1 ; i >= 0 ; i--)
	{
		BA_Delete(list->what_type(), objnums[i]);
	}
}


static void CreateSquare(int model)
{
	int new_sec = BA_New(OBJ_SECTORS);

	if (model >= 0)
		Sectors[new_sec]->RawCopy(Sectors[model]);
	else
		Sectors[new_sec]->SetDefaults();

	int x1 = grid.QuantSnapX(edit.map_x, false);
	int y1 = grid.QuantSnapX(edit.map_y, false);

	int x2 = x1 + new_sector_size;
	int y2 = y1 + new_sector_size;

	for (int i = 0 ; i < 4 ; i++)
	{
		int new_v = BA_New(OBJ_VERTICES);

		Vertices[new_v]->x = (i >= 2) ? x2 : x1;
		Vertices[new_v]->y = (i==1 || i==2) ? y2 : y1;

		int new_sd = BA_New(OBJ_SIDEDEFS);

		SideDefs[new_sd]->SetDefaults(false);
		SideDefs[new_sd]->sector = new_sec;

		int new_ld = BA_New(OBJ_LINEDEFS);

		LineDef * L = LineDefs[new_ld];

		L->start = new_v;
		L->end   = (i == 3) ? (new_v - 3) : new_v + 1;
		L->flags = MLF_Blocking;
		L->right = new_sd;
	}

	// select it
	Selection_Clear();

	edit.Selected->set(new_sec);
}


static void Insert_Thing()
{
	int model = -1;

	if (edit.Selected->notempty())
		model = edit.Selected->find_first();


	BA_Begin();

	int new_t = BA_New(OBJ_THINGS);

	Thing *T = Things[new_t];

	T->type = default_thing;

	if (model >= 0)
		T->RawCopy(Things[model]);

	T->x = grid.SnapX(edit.map_x);
	T->y = grid.SnapY(edit.map_y);

	recent_things.insert_number(T->type);

	BA_End();


	// select it
	Selection_Clear();

	edit.Selected->set(new_t);
}


static void ClosedLoop_Simple(int new_ld, int v2, selection_c& flip)
{
	lineloop_c right_loop;
	lineloop_c  left_loop;

	bool right_ok = TraceLineLoop(new_ld, SIDE_RIGHT, right_loop);
	bool  left_ok = TraceLineLoop(new_ld, SIDE_LEFT,   left_loop);

	// require all lines to be "new" (no sidedefs)
	right_ok = right_ok && right_loop.AllNew();
	 left_ok =  left_ok &&  left_loop.AllNew();

	// check if one of the loops is OK and faces outward.
	// in that case, we just make the island part of the surrounding
	// sector (i.e. we DON'T put a new sector in the inside area).
	// [[ Except when new island contains an island ! ]]

	bool did_outer = false;

	for (int pass = 0 ; pass < 2 ; pass++)
	{
		lineloop_c& loop = (pass == 0) ? right_loop : left_loop;

		bool ok = (pass == 0) ? right_ok : left_ok;

		if (ok && loop.faces_outward)
		{
			int sec_num = loop.FacesSector();

			if (sec_num >= 0)
			{
				AssignSectorToLoop(loop, sec_num, flip);
				did_outer = true;
			}
		}
	}

	// otherwise try to create new sector in the inside area

	// TODO: CONFIG ITEM 'auto_insert_sector'

	for (int pass = 0 ; pass < 2 ; pass++)
	{
		lineloop_c& loop = (pass == 0) ? right_loop : left_loop;

		bool ok = (pass == 0) ? right_ok : left_ok;
		
		if (ok && ! loop.faces_outward)
		{
			loop.FindIslands();

			// if the loop is inside a sector, only _have_ to create
			// the inner sector if we surrounded something.

			if (new_islands_are_void && did_outer && loop.islands.empty())
				return;

			int new_sec = BA_New(OBJ_SECTORS);

			int model = loop.NeighboringSector();

			if (model >= 0)
				Sectors[new_sec]->RawCopy(Sectors[model]);
			else
				Sectors[new_sec]->SetDefaults();

			AssignSectorToLoop(loop, new_sec, flip);
		}
	}
}


static bool TwoNeighboringLineDefs(int new_ld, int v1, int v2,
                                   int *ld1, int *side1,
							       int *ld2, int *side2)
{
	// find the two linedefs that are neighbors to the new line at
	// the second vertex (v2).  The first one (ld1) is on new_ld's
	// right side, and the second one (ld2) is on new_ld's left side.

	*ld1 = -1;
	*ld2 = -1;

	double best_angle1 =  9999;
	double best_angle2 = -9999;

	for (int n = 0 ; n < NumLineDefs ; n++)
	{
		if (n == new_ld)
			continue;

		const LineDef *L = LineDefs[n];

		int other_v;

		if (L->start == v2)
			other_v = L->end;
		else if (L->end == v2)
			other_v = L->start;
		else
			continue;

		double angle = AngleBetweenLines(v1, v2, other_v);

		// overlapping lines
		if (fabs(angle) < 0.0001)
			return false;

		if (angle < best_angle1)
		{
			*ld1 = n;
			*side1 = (other_v == L->start) ? SIDE_LEFT : SIDE_RIGHT;
			best_angle1 = angle;
		}

		if (angle > best_angle2)
		{
			*ld2 = n;
			*side2 = (other_v == L->start) ? SIDE_RIGHT : SIDE_LEFT;
			best_angle2 = angle;
		}
	}

#if 0
	DebugPrintf("best right: line:#%d side:%d angle:%1.2f\n",
	        *ld1, *side1, best_angle1);
	DebugPrintf("best left: line:#%d side:%d angle:%1.2f\n",
	        *ld2, *side2, best_angle2);
#endif

	if (*ld1 < 0 || *ld2 < 0 || *ld1 == *ld2)
		return false;

	return true;
}


static void ClosedLoop_Complex(int new_ld, int v1, int v2, selection_c& flip)
{
DebugPrintf("COMPLEX LOOP : LINE #%d : %d --> %d\n", new_ld, v1, v2);

	// find the two linedefs which are nearest to the new line

	int left_ld = 0,   right_ld = 0;
	int left_side = 0, right_side = 0;

	if (! TwoNeighboringLineDefs(new_ld, v1, v2, &right_ld, &right_side, &left_ld, &left_side))
	{
		// Beep();
		return;
	}

	int right_front = LineDefs[right_ld]->WhatSector(right_side);
	int  left_front = LineDefs[ left_ld]->WhatSector( left_side);

	int right_back = LineDefs[right_ld]->WhatSector(- right_side);
	int  left_back = LineDefs[ left_ld]->WhatSector(-  left_side);

	bool right_new = (right_front < 0) && (right_back < 0);
	bool  left_new = ( left_front < 0) && ( left_back < 0);

DebugPrintf("RIGHT LINE #%d : front=%d back=%d\n", right_ld, right_front, right_back); 
DebugPrintf(" LEFT LINE #%d : front=%d back=%d\n",  left_ld,  left_front,  left_back); 

	if (right_new || left_new)
	{
		// OK
	}
	else if (right_front != left_front)
	{
		// geometry is broken : disable auto-split or auto-sectoring.

		// Beep();
		return;
	}

	// do nothing if touching a "self-referencing linedef"
	// TODO: REVIEW THIS
	if ((right_front >= 0 && right_back == right_front) ||
	    ( left_front >= 0 &&  left_back ==  left_front))
	{
		// Beep();
		return;
	}

	// AT HERE : either splitting a sector or extending one

	lineloop_c right_loop;
	lineloop_c  left_loop;

	// trace the loops on either side of the new line

	bool right_ok = TraceLineLoop(new_ld, SIDE_RIGHT, right_loop);
	bool  left_ok = TraceLineLoop(new_ld, SIDE_LEFT,   left_loop);

DebugPrintf("right_ok : %s\n", right_ok ? "yes" : "NO!");
DebugPrintf(" left_ok : %s\n",  left_ok ? "yes" : "NO!");

if (right_ok) DebugPrintf("right faces outward : %s\n", right_loop.faces_outward ? "YES!" : "no");
if ( left_ok) DebugPrintf(" left faces outward : %s\n",  left_loop.faces_outward ? "YES!" : "no");

	if (right_front >= 0 &&
	    right_front == left_front &&
	    (right_ok && !right_loop.faces_outward) &&
	    ( left_ok && ! left_loop.faces_outward))
	{
		// the SPLITTING case....
		DebugPrintf("SPLITTING sector #%d\n", right_front);

		// TODO: CONFIG ITEM 'auto_split'

		// ensure original sector is OK
		lineloop_c orig_loop;

		if (! TraceLineLoop(right_ld, right_side, orig_loop, true /* ignore_new */))
		{
			DebugPrintf("Traced original : failed\n");
			return;
		}

		if (! orig_loop.SameSector())
		{
			DebugPrintf("Original not all same\n");
			return;
		}

		// OK WE ARE SPLITTING IT : pick which side will stay the same

		double right_total = right_loop.TotalLength();
		double  left_total =  left_loop.TotalLength();

		DebugPrintf("Left total: %1.1f   Right total: %1.1f\n", left_total, right_total);

		lineloop_c&  mod_loop = (left_total < right_total) ? left_loop : right_loop;
		lineloop_c& keep_loop = (left_total < right_total) ? right_loop : left_loop;

		// we'll need the islands too
		mod_loop.FindIslands();

		int new_sec = BA_New(OBJ_SECTORS);

		Sectors[new_sec]->RawCopy(Sectors[right_front]);

		// ensure the new linedef usually ends at v2 (the final vertex)
		if (left_total < right_total)
		{
			AssignSectorToLoop(keep_loop, right_front, flip);
			AssignSectorToLoop( mod_loop, new_sec,     flip);
		}
		else
		{
			AssignSectorToLoop( mod_loop, new_sec,     flip);
			AssignSectorToLoop(keep_loop, right_front, flip);
		}

		return;
	}


	// the SPLIT-VOID case....

	if (right_ok && right_front < 0 && !right_loop.faces_outward &&
		 left_ok &&  left_front < 0 && ! left_loop.faces_outward)
	{
		DebugPrintf("SPLITTING VOID...\n");

		// pick which side gets the new sector

		double right_total = right_loop.TotalLength();
		double  left_total =  left_loop.TotalLength();

		DebugPrintf("Left total: %1.1f   Right total: %1.1f\n", left_total, right_total);

		lineloop_c& loop = (left_total < right_total) ? left_loop : right_loop;

		loop.FindIslands();

		int model = (left_total < right_total) ? left_back : right_back;

		int new_sec = BA_New(OBJ_SECTORS);

		if (model < 0)
			Sectors[new_sec]->SetDefaults();
		else
			Sectors[new_sec]->RawCopy(Sectors[model]);

		AssignSectorToLoop(loop, new_sec, flip);
		return;
	}


	// the EXTENDING case....
	DebugPrintf("EXTENDING....\n");

	// TODO: CONFIG ITEM 'auto_extend'

	for (int pass = 0 ; pass < 2 ; pass++)
	{
		lineloop_c& loop = (pass == 0) ? right_loop : left_loop;

		int front = (pass == 0) ? right_front : left_front;

		bool ok = (pass == 0) ? right_ok : left_ok;

		// bad geometry?
		if (! ok)
			continue;

		if (! loop.faces_outward)
		{
			loop.FindIslands();
DebugPrintf("ISLANDS = %u\n", loop.islands.size());

			int model = loop.NeighboringSector();

			int new_sec = BA_New(OBJ_SECTORS);

			if (model < 0)
				Sectors[new_sec]->SetDefaults();
			else
				Sectors[new_sec]->RawCopy(Sectors[model]);

			AssignSectorToLoop(loop, new_sec, flip);
		}
		else
		{
			// when front >= 0, we can be certain we are extending an
			// island within an existing sector.  When < 0, we check
			// whether the loop can see an outer sector.

			int sec_num = (front >= 0) ? front : loop.FacesSector();

			if (sec_num >= 0)
			{
				AssignSectorToLoop(loop, sec_num, flip);
			}
		}
	}
}


void Insert_LineDef(int v1, int v2, bool no_fill = false)
{
	int new_ld = BA_New(OBJ_LINEDEFS);

	LineDef * L = LineDefs[new_ld];

	L->start = v1;
	L->end   = v2;
	L->flags = MLF_Blocking;

	if (no_fill)
		return;

	selection_c flip(OBJ_LINEDEFS);

	switch (VertexHowManyLineDefs(v2))
	{
		case 0:
			// this should not happen!

		case 1:
			// joined onto an isolated vertex : nothing to do
			return;

		case 2:
			ClosedLoop_Simple(new_ld, v2, flip);
			break;

		default:  // 3 or more
			ClosedLoop_Complex(new_ld, v1, v2, flip);
			break;
	}

	FlipLineDefGroup(flip);
}


void Insert_LineDef_autosplit(int v1, int v2, bool no_fill = false)
{
	if (LineDefAlreadyExists(v1, v2))
		return;

fprintf(stderr, "Insert_LineDef_autosplit %d..%d\n", v1, v2);

	// Find a linedef which this new line would cross, and if it exists
	// add a vertex there and create TWO lines.  Also handle a vertex
	// that this line crosses (sits on) similarly.

	cross_state_t cross;

	if (! FindClosestCrossPoint(v1, v2, &cross))
	{
		Insert_LineDef(v1, v2, no_fill);
		return;
	}

	if (cross.line >= 0)
	{
		cross.vert = BA_New(OBJ_VERTICES);

		Vertex *V = Vertices[cross.vert];

		V->x = cross.x;
		V->y = cross.y;

		SplitLineDefAtVertex(cross.line, cross.vert);
	}

	// recursively handle both sides
	
	Insert_LineDef_autosplit(v1, cross.vert, no_fill);
	Insert_LineDef_autosplit(cross.vert, v2, no_fill);
}


void Insert_Vertex(bool force_select, bool no_fill)
{
	int reselect = true;

	if (edit.Selected->count_obj() > 2)
	{
		Beep("Too many vertices to add a linedef");
		return;
	}


	// the "nearby" vertex is usually the highlighted one (if any),
	// but we substitute the vertex at the snapped coord when the
	// highlight is empty and snapping is enabled.
	int near_vert = -1;

	if (edit.highlight.valid())
		near_vert = edit.highlight.num;

	int new_x = grid.SnapX(edit.map_x);
	int new_y = grid.SnapY(edit.map_y);

	if (near_vert < 0 && grid.snap)
		near_vert = Vertex_FindExact(new_x, new_y);


	// if a vertex is highlighted but none are selected, then merely
	// select that vertex.  This is better than adding a new vertex at
	// the same location as an existing one.
	if (edit.Selected->empty() && near_vert >= 0)
	{
		edit.Selected->set(near_vert);

		if (easier_drawing_mode)
		{
			Editor_SetAction(ACT_DRAW_LINE);
			edit.drawing_from = near_vert;
		}
		return;
	}


	int first_sel  = edit.Selected->find_first();
	int second_sel = edit.Selected->find_second();

	// if highlighted vertex same as selected one, merely deselect it
	if (second_sel < 0 && near_vert >= 0 && near_vert == first_sel)
	{
		edit.Selected->clear(first_sel);
		Editor_ClearAction();
		return;
	}

	// if only one is selected, but another is highlighted, use the
	// highlighted one as the second vertex
	if (second_sel < 0 && near_vert >= 0)
	{
		second_sel = near_vert;
	}

	// insert a new linedef between two existing vertices
	if (first_sel >= 0 && second_sel >= 0)
	{
		if (LineDefAlreadyExists(first_sel, second_sel))
		{
			edit.Selected->set(first_sel);
			edit.Selected->set(second_sel);

			Editor_ClearAction();
			return;
		}

		if (!force_select && VertexHowManyLineDefs(second_sel) > 0)
			reselect = false;

		BA_Begin();

		Insert_LineDef_autosplit(first_sel, second_sel, no_fill);

		BA_End();

		Selection_Clear();
		Editor_ClearAction();

		if (reselect)
		{
			edit.Selected->set(second_sel);

			if (easier_drawing_mode)
			{
				Editor_SetAction(ACT_DRAW_LINE);
				edit.drawing_from = second_sel;
			}
		}

		return;
	}

	// make sure we never create zero-length linedefs

	if (first_sel >= 0)
	{
		Vertex *V = Vertices[first_sel];

		if (V->Matches(new_x, new_y))
		{
			Selection_Clear();
			Editor_ClearAction();
			return;
		}
	}


	// handle case of splitting a line where the first selected vertex
	// matches an endpoint of that line -- we just want to split the
	// line then (NOT insert a new linedef)
	if (first_sel >= 0 && edit.split_line.valid() &&
	    (first_sel == LineDefs[edit.split_line.num]->start ||
		 first_sel == LineDefs[edit.split_line.num]->end))
	{
		first_sel = -1;
	}


#if 0
	// prevent adding a linedef that would overlap an existing one
	if (first_sel >= 0 && LineDefWouldOverlap(first_sel, new_x, new_y))
	{
		Beep("New linedef would overlap another");
		return;
	}
#endif


	BA_Begin();

	int new_v = BA_New(OBJ_VERTICES);

	Vertex *V = Vertices[new_v];

	V->x = new_x;
	V->y = new_y;

	// split an existing linedef?
	if (edit.split_line.valid())
	{
		V->x = edit.split_x;
		V->y = edit.split_y;

		SplitLineDefAtVertex(edit.split_line.num, new_v);

		if (!force_select && first_sel >= 0)
			reselect = false;
	}

	// add a new linedef?
	if (first_sel >= 0)
	{
		Insert_LineDef_autosplit(first_sel, new_v, no_fill);
	}

	BA_End();

	// select new vertex
	Selection_Clear();
	Editor_ClearAction();

	if (reselect)
	{
		edit.Selected->set(new_v);

		if (easier_drawing_mode)
		{
			Editor_SetAction(ACT_DRAW_LINE);
			edit.drawing_from = new_v;
		}
	}

	RedrawMap();
}


static void Correct_Sector(int sec_num)
{
	BA_Begin();

	AssignSectorToSpace(edit.map_x, edit.map_y, sec_num);

	BA_End();
}


static void Insert_Sector(bool force_new)
{
	int sel_count = edit.Selected->count_obj();
	if (sel_count > 1)
	{
		Beep("Too many sectors to copy from");
		return;
	}

	// if outside of the map, create a square 
	if (PointOutsideOfMap(edit.map_x, edit.map_y))
	{
		BA_Begin();

		int model = -1;
		if (sel_count > 0)
			model = edit.Selected->find_first();

		CreateSquare(model);

		BA_End();

		return;
	}

	// if a sector is highlighted, merely correct it (unless CTRL is pressed)
	if (edit.highlight.valid() && ! force_new)
	{
		// must not be any selection
		if (sel_count > 0)
		{
			Beep("Correct sector not supported on selection");
			return;
		}

		Correct_Sector(edit.highlight.num);
		return;
	}

	// --- adding a NEW sector to the area ---

	// determine a model sector to copy properties from
	int model;

	if (sel_count > 0)
		model = edit.Selected->find_first();
	else if (edit.highlight.valid())
		model = edit.highlight.num;
	else
		model = -1;  // look for a neighbor to copy


	BA_Begin();

	int new_sec = BA_New(OBJ_SECTORS);

	if (model >= 0)
	{
		Sectors[new_sec]->RawCopy(Sectors[model]);
	}

	AssignSectorToSpace(edit.map_x, edit.map_y, new_sec, model < 0);

	BA_End();


	Selection_Clear();

	edit.Selected->set(new_sec);
}


void CMD_Insert(void)
{
	bool force_new;
	bool force_select;
	bool no_fill;

	switch (edit.mode)
	{
		case OBJ_THINGS:
			Insert_Thing();
			break;

		case OBJ_VERTICES:
			force_select = Exec_HasFlag("/select");
			no_fill      = Exec_HasFlag("/nofill");
			Insert_Vertex(force_select, no_fill);
			break;

		case OBJ_SECTORS:
			force_new = Exec_HasFlag("/new");
			Insert_Sector(force_new);
			break;

		default:
			Beep("Cannot insert in this mode");
			break;
	}

	RedrawMap();
}


/*
   check if a (part of a) LineDef is inside a given box
*/
bool LineTouchesBox (int ld, int x0, int y0, int x1, int y1)
{
	int lx0 = LineDefs[ld]->Start()->x;
	int ly0 = LineDefs[ld]->Start()->y;
	int lx1 = LineDefs[ld]->End()->x;
	int ly1 = LineDefs[ld]->End()->y;

	int i;

	// start is entirely inside the square?
	if (lx0 >= x0 && lx0 <= x1 && ly0 >= y0 && ly0 <= y1)
		return true;

	// end is entirely inside the square?
	if (lx1 >= x0 && lx1 <= x1 && ly1 >= y0 && ly1 <= y1)
		return true;
	

	if ((ly0 > y0) != (ly1 > y0))
	{
		i = lx0 + (int) ((double) (y0 - ly0) * (double) (lx1 - lx0) / (double) (ly1 - ly0));
		if (i >= x0 && i <= x1)
			return true; /* the linedef crosses the left side */
	}
	if ((ly0 > y1) != (ly1 > y1))
	{
		i = lx0 + (int) ((double) (y1 - ly0) * (double) (lx1 - lx0) / (double) (ly1 - ly0));
		if (i >= x0 && i <= x1)
			return true; /* the linedef crosses the right side */
	}
	if ((lx0 > x0) != (lx1 > x0))
	{
		i = ly0 + (int) ((double) (x0 - lx0) * (double) (ly1 - ly0) / (double) (lx1 - lx0));
		if (i >= y0 && i <= y1)
			return true; /* the linedef crosses the bottom side */
	}
	if ((lx0 > x1) != (lx1 > x1))
	{
		i = ly0 + (int) ((double) (x1 - lx0) * (double) (ly1 - ly0) / (double) (lx1 - lx0));
		if (i >= y0 && i <= y1)
			return true; /* the linedef crosses the top side */
	}

	return false;
}



static void DoMoveObjects(selection_c *list, int delta_x, int delta_y, int delta_z)
{
	selection_iterator_c it;

	switch (list->what_type())
	{
		case OBJ_THINGS:
			for (list->begin(&it) ; !it.at_end() ; ++it)
			{
				const Thing * T = Things[*it];

				BA_ChangeTH(*it, Thing::F_X, T->x + delta_x);
				BA_ChangeTH(*it, Thing::F_Y, T->y + delta_y);
			}
			break;

		case OBJ_VERTICES:
			for (list->begin(&it) ; !it.at_end() ; ++it)
			{
				const Vertex * V = Vertices[*it];

				BA_ChangeVT(*it, Vertex::F_X, V->x + delta_x);
				BA_ChangeVT(*it, Vertex::F_Y, V->y + delta_y);
			}
			break;

		case OBJ_SECTORS:
			// apply the Z delta first
			for (list->begin(&it) ; !it.at_end() ; ++it)
			{
				const Sector * S = Sectors[*it];

				BA_ChangeSEC(*it, Sector::F_FLOORH, S->floorh + delta_z);
				BA_ChangeSEC(*it, Sector::F_CEILH,  S->ceilh  + delta_z);
			}

			/* FALL-THROUGH !! */

		case OBJ_LINEDEFS:
			{
				selection_c verts(OBJ_VERTICES);

				ConvertSelection(list, &verts);

				DoMoveObjects(&verts, delta_x, delta_y, delta_z);
			}
			break;

		default:
			break;
	}
}


void CMD_MoveObjects(int delta_x, int delta_y, int delta_z)
{
	if (edit.Selected->empty())
		return;

	BA_Begin();

	// handle a single vertex merging onto an existing one
	if (edit.mode == OBJ_VERTICES && edit.drag_single_vertex >= 0 &&
	    edit.highlight.valid())
	{
		MergeVertex(edit.drag_single_vertex, edit.highlight.num,
		            true /* v1_will_be_deleted */);

		BA_Delete(OBJ_VERTICES, edit.drag_single_vertex);

		edit.drag_single_vertex = -1;

		goto success;
	}

	// handle a single vertex splitting a linedef
	if (edit.mode == OBJ_VERTICES && edit.drag_single_vertex >= 0 &&
		edit.split_line.valid())
	{
		SplitLineDefAtVertex(edit.split_line.num, edit.drag_single_vertex);

		// now move the vertex!
	}

	// move things in sectors too (must do it _before_ moving the
	// sectors, otherwise we fail trying to determine which sectors
	// each thing is in).
	if (edit.mode == OBJ_SECTORS)
	{
		selection_c thing_sel(OBJ_THINGS);

		ConvertSelection(edit.Selected, &thing_sel);

		DoMoveObjects(&thing_sel, delta_x, delta_y, delta_z);
	}

	DoMoveObjects(edit.Selected, delta_x, delta_y, delta_z);

success:
	BA_End();
}


static void TransferThingProperties(int src_thing, int dest_thing)
{
	const Thing * T = Things[src_thing];

	BA_ChangeTH(dest_thing, Thing::F_TYPE,    T->type);
	BA_ChangeTH(dest_thing, Thing::F_OPTIONS, T->options);
//	BA_ChangeTH(dest_thing, Thing::F_ANGLE,   T->angle);

	BA_ChangeTH(dest_thing, Thing::F_TID,     T->tid);
	BA_ChangeTH(dest_thing, Thing::F_SPECIAL, T->special);

	BA_ChangeTH(dest_thing, Thing::F_ARG1, T->arg1);
	BA_ChangeTH(dest_thing, Thing::F_ARG2, T->arg2);
	BA_ChangeTH(dest_thing, Thing::F_ARG3, T->arg3);
	BA_ChangeTH(dest_thing, Thing::F_ARG4, T->arg4);
	BA_ChangeTH(dest_thing, Thing::F_ARG5, T->arg5);
}


static void TransferSectorProperties(int src_sec, int dest_sec)
{
	const Sector * SEC = Sectors[src_sec];

	BA_ChangeSEC(dest_sec, Sector::F_FLOORH,    SEC->floorh);
	BA_ChangeSEC(dest_sec, Sector::F_FLOOR_TEX, SEC->floor_tex);
	BA_ChangeSEC(dest_sec, Sector::F_CEILH,     SEC->ceilh);
	BA_ChangeSEC(dest_sec, Sector::F_CEIL_TEX,  SEC->ceil_tex);

	BA_ChangeSEC(dest_sec, Sector::F_LIGHT,  SEC->light);
	BA_ChangeSEC(dest_sec, Sector::F_TYPE,   SEC->type);
	BA_ChangeSEC(dest_sec, Sector::F_TAG,    SEC->tag);
}


#define LINEDEF_FLAG_KEEP  (MLF_Blocking + MLF_TwoSided)

static void TransferLinedefProperties(int src_line, int dest_line, bool do_tex)
{
	const LineDef * L1 = LineDefs[src_line];
	const LineDef * L2 = LineDefs[dest_line];

	// don't transfer certain flags
	int flags = LineDefs[dest_line]->flags;
	flags = (flags & LINEDEF_FLAG_KEEP) | (L1->flags & ~LINEDEF_FLAG_KEEP);

	// handle textures
	if (do_tex && L1->Right() && L2->Right())
	{
		/* There are four cases, depending on number of sides: 
		 *
		 * (a) single --> single : easy
		 *
		 * (b) single --> double : copy mid_tex to both sides upper and lower
		 *                         [alternate idea: copy mid_tex to VISIBLE sides]
		 *
		 * (c) double --> single : pick a texture (e.g. visible lower) to copy
		 *
		 * (d) double --> double : copy each side, but possibly flip the
		 *                         second linedef based on floor or ceil diff.
		 */
		if (! L1->Left())
		{
			int tex = L1->Right()->mid_tex;

			if (! L2->Left())
			{
				BA_ChangeSD(L2->right, SideDef::F_MID_TEX, tex);
			}
			else
			{
				BA_ChangeSD(L2->right, SideDef::F_LOWER_TEX, tex);
				BA_ChangeSD(L2->right, SideDef::F_UPPER_TEX, tex);

				BA_ChangeSD(L2->left,  SideDef::F_LOWER_TEX, tex);
				BA_ChangeSD(L2->left,  SideDef::F_UPPER_TEX, tex);

				// this is debatable....   CONFIG ITEM?
				flags |= MLF_LowerUnpegged;
				flags |= MLF_UpperUnpegged;
			}
		}
		else if (! L2->Left())
		{
			/* pick which texture to copy */

			const Sector *front = L1->Right()->SecRef();
			const Sector *back  = L1-> Left()->SecRef();

			int f_l = L1->Right()->lower_tex;
			int f_u = L1->Right()->upper_tex;
			int b_l = L1-> Left()->lower_tex;
			int b_u = L1-> Left()->upper_tex;

			// ignore missing textures
			if (BA_GetString(f_l)[0] == '-') f_l = 0;
			if (BA_GetString(f_u)[0] == '-') f_u = 0;
			if (BA_GetString(b_l)[0] == '-') b_l = 0;
			if (BA_GetString(b_u)[0] == '-') b_u = 0;

			// try hard to find a usable texture
			int tex = -1;

				 if (front->floorh < back->floorh && f_l > 0) tex = f_l;
			else if (front->floorh > back->floorh && b_l > 0) tex = b_l;
			else if (front-> ceilh > back-> ceilh && f_u > 0) tex = f_u;
			else if (front-> ceilh < back-> ceilh && b_u > 0) tex = b_u;
			else if (f_l > 0) tex = f_l;
			else if (b_l > 0) tex = b_l;
			else if (f_u > 0) tex = f_u;
			else if (b_u > 0) tex = b_u;

			if (tex > 0)
			{
				BA_ChangeSD(L2->right, SideDef::F_MID_TEX, tex);
			}
		}
		else
		{
			const SideDef *RS = L1->Right();
			const SideDef *LS = L1->Left();

			const Sector *F1 = L1->Right()->SecRef();
			const Sector *B1 = L1-> Left()->SecRef();
			const Sector *F2 = L2->Right()->SecRef();
			const Sector *B2 = L2-> Left()->SecRef();

			// logic to determine which sides we copy 

			int f_diff1 = B1->floorh - F1->floorh;
			int f_diff2 = B2->floorh - F2->floorh;
			int c_diff1 = B1->ceilh  - F1->ceilh;
			int c_diff2 = B2->ceilh  - F2->ceilh;

			if (f_diff1 * f_diff2 > 0)
			  { /* no change */ }
			else if (f_diff1 * f_diff2 < 0)
			  std::swap(LS, RS);
			else if (c_diff1 * c_diff2 > 0)
			  { /* no change */ }
			else if (c_diff1 * c_diff2 < 0)
			  std::swap(LS, RS);
			else if (L1->start == L2->end || L1->end == L2->start)
			  { /* no change */ }
			else if (L1->start == L2->start || L1->end == L2->end)
			  std::swap(LS, RS);
			else if (F1 == F2 || B1 == B2)
			  { /* no change */ }
			else if (F1 == B1 || F1 == B2 || F2 == B1 || F2 == B2)
			  std::swap(LS, RS);

			// TODO; review if we should copy '-' into lowers or uppers

			BA_ChangeSD(L2->right, SideDef::F_LOWER_TEX, RS->lower_tex);
			BA_ChangeSD(L2->right, SideDef::F_MID_TEX,   RS->mid_tex);
			BA_ChangeSD(L2->right, SideDef::F_UPPER_TEX, RS->upper_tex);

			BA_ChangeSD(L2->left, SideDef::F_LOWER_TEX, LS->lower_tex);
			BA_ChangeSD(L2->left, SideDef::F_MID_TEX,   LS->mid_tex);
			BA_ChangeSD(L2->left, SideDef::F_UPPER_TEX, LS->upper_tex);
		}
	}

	BA_ChangeLD(dest_line, LineDef::F_FLAGS, flags);

	BA_ChangeLD(dest_line, LineDef::F_TYPE, L1->type);
	BA_ChangeLD(dest_line, LineDef::F_TAG,  L1->tag);

	BA_ChangeLD(dest_line, LineDef::F_ARG2, L1->arg2);
	BA_ChangeLD(dest_line, LineDef::F_ARG3, L1->arg3);
	BA_ChangeLD(dest_line, LineDef::F_ARG4, L1->arg4);
	BA_ChangeLD(dest_line, LineDef::F_ARG5, L1->arg5);
}


void CMD_CopyProperties(void)
{
	if (edit.highlight.is_nil())
	{
		Beep("No target for CopyProperties");
		return;
	}
	else if (edit.Selected->empty())
	{
		Beep("No source for CopyProperties");
		return;
	}
	else if (edit.mode == OBJ_VERTICES)
	{
		Beep("No properties to copy");
		return;
	}


	/* normal mode, SEL --> HILITE */

	if (! Exec_HasFlag("/reverse"))
	{
		if (edit.Selected->count_obj() != 1)
		{
			Beep("Too many sources for CopyProperties");
			return;
		}

		int source = edit.Selected->find_first();
		int target = edit.highlight.num;

		// silently allow copying onto self
		if (source == target)
			return;

		BA_Begin();

		switch (edit.mode)
		{
			case OBJ_SECTORS:
				TransferSectorProperties(source, target);
				break;

			case OBJ_THINGS:
				TransferThingProperties(source, target);
				break;

			case OBJ_LINEDEFS:
				TransferLinedefProperties(source, target, true /* do_tex */);
				break;

			default: break;
		}

		BA_End();

	}
	else  /* reverse mode, HILITE --> SEL */
	{
		if (edit.Selected->count_obj() == 1 && edit.Selected->find_first() == edit.highlight.num)
		{
			Beep("No selection for CopyProperties");
			return;
		}

		int source = edit.highlight.num;

		selection_iterator_c it;

		BA_Begin();

		for (edit.Selected->begin(&it) ; !it.at_end() ; ++it)
		{
			if (*it == source)
				continue;

			switch (edit.mode)
			{
				case OBJ_SECTORS:
					TransferSectorProperties(source, *it);
					break;

				case OBJ_THINGS:
					TransferThingProperties(source, *it);
					break;

				case OBJ_LINEDEFS:
					TransferLinedefProperties(source, *it, true /* do_tex */);
					break;

				default: break;  // fuck you, compiler
			}
		}

		BA_End();
	}
}



static void Drag_CountOnGrid_Worker(int obj_type, int objnum, int *count, int *total)
{
	switch (obj_type)
	{
		case OBJ_THINGS:
			*total += 1;
			if (grid.OnGrid(Things[objnum]->x, Things[objnum]->y))
				*count += 1;
			break;

		case OBJ_VERTICES:
			*total += 1;
			if (grid.OnGrid(Vertices[objnum]->x, Vertices[objnum]->y))
				*count += 1;
			break;

		case OBJ_LINEDEFS:
			Drag_CountOnGrid_Worker(OBJ_VERTICES, LineDefs[objnum]->start, count, total);
			Drag_CountOnGrid_Worker(OBJ_VERTICES, LineDefs[objnum]->end,   count, total);
			break;

		case OBJ_SECTORS:
			for (int n = 0 ; n < NumLineDefs ; n++)
			{
				LineDef *L = LineDefs[n];

				if (! L->TouchesSector(objnum))
					continue;

				Drag_CountOnGrid_Worker(OBJ_LINEDEFS, n, count, total);
			}
			break;

		default:
			break;
	}
}


static void Drag_CountOnGrid(int *count, int *total)
{
	// Note: the results are approximate, vertices can be counted two
	//       or more times.

	selection_iterator_c it;

	for (edit.Selected->begin(&it) ; !it.at_end() ; ++it)
	{
		Drag_CountOnGrid_Worker(edit.mode, *it, count, total);
	}
}


static void Drag_UpdateObjectDist(int obj_type, int objnum, int *x, int *y,
                                  int *best_dist, int map_x, int map_y,
								  bool only_grid)
{
	int x2, y2;

	switch (obj_type)
	{
		case OBJ_THINGS:
			x2 = Things[objnum]->x;
			y2 = Things[objnum]->y;
			break;

		case OBJ_VERTICES:
			x2 = Vertices[objnum]->x;
			y2 = Vertices[objnum]->y;
			break;

		case OBJ_LINEDEFS:
			{
				LineDef *L = LineDefs[objnum];

				Drag_UpdateObjectDist(OBJ_VERTICES, L->start, x, y, best_dist,
									  map_x, map_y, only_grid);

				Drag_UpdateObjectDist(OBJ_VERTICES, L->end,   x, y, best_dist,
				                      map_x, map_y, only_grid);
			}
			return;

		case OBJ_SECTORS:
			// recursively handle all vertices belonging to the sector
			// (some vertices can be processed two or more times, that
			// won't matter though).

			for (int n = 0 ; n < NumLineDefs ; n++)
			{
				LineDef *L = LineDefs[n];

				if (! L->TouchesSector(objnum))
					continue;

				Drag_UpdateObjectDist(OBJ_LINEDEFS, n, x, y, best_dist,
				                      map_x, map_y, only_grid);
			}
			return;

		default:
			return;
	}

	// handle OBJ_THINGS and OBJ_VERTICES

	if (only_grid && ! grid.OnGrid(x2, y2))
		return;

	int dist = ComputeDist(x2 - map_x, y2 - map_y);

	if (dist < *best_dist)
	{
		*x = x2;
		*y = y2;

		*best_dist = dist;
	}
}


void GetDragFocus(int *x, int *y, int map_x, int map_y)
{
	*x = 0;
	*y = 0;

	// determine whether a majority of the object(s) are already on
	// the grid.  If they are, then pick a coordinate that also lies
	// on the grid.
	bool only_grid = false;

	int count = 0;
	int total = 0;

	if (grid.snap)
	{
		Drag_CountOnGrid(&count, &total);

		if (total > 0 && count > total / 2)
			only_grid = true;
	}

	int best_dist = 99999;

	selection_iterator_c it;

	for (edit.Selected->begin(&it) ; !it.at_end() ; ++it)
	{
		Drag_UpdateObjectDist(edit.mode, *it, x, y, &best_dist,
		                      map_x, map_y, only_grid);
	}
}


bool Texture_MatchPattern(const char *tex, const char *pattern)
{
	// Note: an empty pattern matches NOTHING

	char local_pat[256];
	local_pat[0] = 0;


	// add '*' to the start and end of the pattern
	// (unless it uses the ^ or $ anchors)

	bool negated = false;
	if (pattern[0] == '!')
	{
		pattern++;
		negated = true;
	}

	if (pattern[0] == '^')
		pattern++;
	else
		strcpy(local_pat, "*");
	
	strcat(local_pat, pattern);

	size_t len = strlen(local_pat);

	if (len == 0)
		return false;

	if (local_pat[len-1] == '$')
		local_pat[len-1] = 0;
	else
		strcat(local_pat, "*");

	bool result = fl_filename_match(tex, local_pat) ? true : false;

	return negated ? !result : result;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
