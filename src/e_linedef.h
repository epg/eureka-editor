//------------------------------------------------------------------------
//  LINEDEF OPERATIONS
//------------------------------------------------------------------------
//
//  Eureka DOOM Editor
//
//  Copyright (C) 2001-2009 Andrew Apted
//  Copyright (C) 1997-2003 André Majorel et al
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
//  in the public domain in 1994 by Raphaël Quinet and Brendon Wyber.
//
//------------------------------------------------------------------------

#ifndef __EUREKA_E_LINEDEF_H__
#define __EUREKA_E_LINEDEF_H__

class bitvec_c;
#include "selectn.h"

void FlipLineDef(int ld);
void FlipLineDefGroup(selection_c& flip);

void CMD_FlipLineDefs();
void CMD_SplitLineDefs();

void CMD_AlignTexturesX();
void CMD_AlignTexturesY();


int SplitLineDefAtVertex(int ld, int v_idx);

void MoveCoordOntoLineDef(int ld, int *x, int *y);


bitvec_c *bv_vertices_of_linedefs (bitvec_c *linedefs);
bitvec_c *bv_vertices_of_linedefs (SelPtr list);
SelPtr list_vertices_of_linedefs (SelPtr list);

void centre_of_linedefs (selection_c * list, int *x, int *y);

void frob_linedefs_flags (selection_c * list, int op, int operand);


void MakeRectangularNook (SelPtr obj, int width, int depth, int convex);
void SetLinedefLength (SelPtr obj, int length, int move_2nd_vertex);

#endif  /* __EUREKA_E_LINEDEF_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
