//------------------------------------------------------------------------
//  HIGHLIGHT HELPER
//------------------------------------------------------------------------
//
//  Eureka DOOM Editor
//
//  Copyright (C) 2001-2019 Andrew Apted
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

#ifndef __EUREKA_X_HOVER_H__
#define __EUREKA_X_HOVER_H__

#include "DocumentModule.h"

class bitvec_c;
class crossing_state_c;
class fastopp_node_c;
class LineDef;
class Objid;
enum class Side;
struct v2double_t;

namespace hover
{
int getClosestLine_CastingHoriz(const Document &doc, v2double_t pos, Side *side);
}

//
// The hover module
//
class Hover : public DocumentModule
{
public:
	Hover(Document &doc) : DocumentModule(doc)
	{
	}

	Objid getNearbyObject(ObjType type, const v2double_t &pos) const;

	Objid findSplitLine(v2double_t &out, const v2double_t &ptr, int ignore_vert) const;
	Objid findSplitLineForDangler(int v_num) const;

	int getOppositeLinedef(int ld, Side ld_side, Side *result_side, const bitvec_c *ignore_lines) const;
	int getOppositeSector(int ld, Side ld_side) const;
	void fastOpposite_begin();
	void fastOpposite_finish();

	bool isPointOutsideOfMap(double x, double y) const;

	void findCrossingPoints(crossing_state_c &cross,
		v2double_t p1, int possible_v1,
		v2double_t p2, int possible_v2) const;

private:
	Objid getNearestSplitLine(const v2double_t &pos, int ignore_vert) const;

	void findCrossingLines(crossing_state_c &cross, double x1, double y1, int possible_v1, double x2, double y2, int possible_v2) const;

	fastopp_node_c *m_fastopp_X_tree = nullptr;
	fastopp_node_c *m_fastopp_Y_tree = nullptr;
};

// result: -1 for back, +1 for front, 0 for _exactly_on_ the line
Side PointOnLineSide(double x, double y, double lx1, double ly1, double lx2, double ly2);


struct cross_point_t
{
	int vert;	// >= 0 when we hit a vertex
	int ld;     // >= 0 when we hit a linedef instead

	double x, y;	// coordinate of line split point
	double dist;
};


class crossing_state_c
{
public:
	std::vector< cross_point_t > points;

	// the start/end coordinates of the whole tested line
	double start_x = 0, start_y = 0;
	double   end_x = 0,   end_y = 0;

	Instance &inst;

public:
	explicit crossing_state_c(Instance &inst) : inst(inst)
	{
	}

	void clear();

	void add_vert(int v, double dist);
	void add_line(int ld, double new_x, double new_y, double dist);

	bool HasVertex(int v) const;
	bool HasLine(int ld)  const;

	void Sort();

	void SplitAllLines(EditOperation &op);

private:
	struct point_CMP
	{
		inline bool operator() (const cross_point_t &A, const cross_point_t& B) const
		{
			return A.dist < B.dist;
		}
	};
};

#endif  /* __EUREKA_X_HOVER_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
