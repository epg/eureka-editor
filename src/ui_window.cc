//------------------------------------------------------------------------
//  Main Window
//------------------------------------------------------------------------
//
//  Eureka DOOM Editor
//
//  Copyright (C) 2006-2013 Andrew Apted
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
#include "ui_window.h"
#include "w_wad.h"

#ifndef WIN32
#include <unistd.h>
#endif

#if (FL_MAJOR_VERSION != 1 ||  \
     FL_MINOR_VERSION != 3 ||  \
     FL_PATCH_VERSION < 0)
#error "Require FLTK version 1.3.0 or later"
#endif



UI_MainWin *main_win;

#define MAIN_WINDOW_W  (800-32+KF*60)
#define MAIN_WINDOW_H  (600-98+KF*40)

#define MAX_WINDOW_W  MAIN_WINDOW_W
#define MAX_WINDOW_H  MAIN_WINDOW_H


static void main_win_close_CB(Fl_Widget *w, void *data)
{
	CMD_Quit();
}


//
// MainWin Constructor
//
UI_MainWin::UI_MainWin() :
	Fl_Double_Window(MAIN_WINDOW_W, MAIN_WINDOW_H, EUREKA_TITLE),
	cursor_shape(FL_CURSOR_DEFAULT),
	last_x(0), last_y(0), last_w(0), last_h(0)
{
	end(); // cancel begin() in Fl_Group constructor

	size_range(MAIN_WINDOW_W, MAIN_WINDOW_H);

	callback((Fl_Callback *) main_win_close_CB);

	color(WINDOW_BG, WINDOW_BG);

	int cy = 0;
	int ey = h();

	panel_W = 260 + KF * 32;

	/* ---- Menu bar ---- */
	{
		menu_bar = Menu_Create(0, 0, w()-3 - panel_W, 28+KF*3);
		add(menu_bar);

#ifndef __APPLE__
		cy += menu_bar->h();
#endif
	}


	info_bar = new UI_InfoBar(0, ey - (28+KF*3), w(), 28+KF*3);
	add(info_bar);

	ey = ey - info_bar->h();


	int browser_W = MIN_BROWSER_W + 56;

	int cw = w() - panel_W - browser_W;
	int ch = ey - cy;

	scroll = new UI_CanvasScroll(0, cy, cw, ch);

	canvas = scroll->canvas;
	render = scroll->render;

	browser = new UI_Browser(w() - panel_W - browser_W, cy, browser_W, ey - cy);

	tile = new UI_Tile(0, cy, w() - panel_W, ey - cy, NULL, scroll, browser);
	add(tile);

	resizable(tile);


	int BY = 0;     // cy+2
	int BH = ey-4;  // ey-BY-2

	thing_box = new UI_ThingBox(w() - panel_W, BY, panel_W, BH);
	add(thing_box);

	line_box = new UI_LineBox(w() - panel_W, BY, panel_W, BH);
	line_box->hide();
	add(line_box);

	sec_box = new UI_SectorBox(w() - panel_W, BY, panel_W, BH);
	sec_box->hide();
	add(sec_box);

	vert_box = new UI_VertexBox(w() - panel_W, BY, panel_W, BH);
	vert_box->hide();
	add(vert_box);
}

//
// MainWin Destructor
//
UI_MainWin::~UI_MainWin()
{ }


void UI_MainWin::NewEditMode(char mode)
{
	UnselectPics();

	thing_box->hide();
	 line_box->hide();
	  sec_box->hide();
	 vert_box->hide();

	switch (mode)
	{
		case 't': thing_box->show(); break;
		case 'l':  line_box->show(); break;
		case 's':   sec_box->show(); break;
		case 'v':  vert_box->show(); break;

		default: break;
	}

	info_bar->NewEditMode(mode);
	browser ->NewEditMode(mode);

	redraw();
}


void UI_MainWin::SetCursor(Fl_Cursor shape)
{
	if (shape == cursor_shape)
		return;

	cursor_shape = shape;

	cursor(shape);
}


void UI_MainWin::ShowBrowser(char kind)
{
	bool is_visible = browser->visible() ? true : false;

	if (kind == '-' || (is_visible && kind == '/'))
		kind = 0;

	bool want_visible = (kind != 0) ? true : false;

	if (is_visible != want_visible)
	{
		if (want_visible)
			tile->ShowRight();
		else
			tile->HideRight();

		// hiding the browser also clears any pic selection
		if (! want_visible)
			UnselectPics();
	}

	if (kind != 0 && kind != '/')
	{
		browser->ChangeMode(kind);
	}
}


void UI_MainWin::UpdateTotals()
{
	thing_box->UpdateTotal();
	 line_box->UpdateTotal();
	  sec_box->UpdateTotal();
	 vert_box->UpdateTotal();
}


int UI_MainWin::GetPanelObjNum() const
{
	// FIXME: using 'edit' here feels like a hack or mis-design
	switch (edit.mode)
	{
		case OBJ_THINGS:   return thing_box->GetObj();
		case OBJ_VERTICES: return  vert_box->GetObj();
		case OBJ_SECTORS:  return   sec_box->GetObj();
		case OBJ_LINEDEFS: return  line_box->GetObj();

		default:
			return -1;
	}
}

void UI_MainWin::InvalidatePanelObj()
{
	if (thing_box->visible()) 
		thing_box->SetObj(-1, 0);

	if (line_box->visible())
		line_box->SetObj(-1, 0);

	if (sec_box->visible())
		sec_box->SetObj(-1, 0);

	if (vert_box->visible())
		vert_box->SetObj(-1, 0);
}

void UI_MainWin::UpdatePanelObj()
{
	if (thing_box->visible()) 
		thing_box->UpdateField();

	if (line_box->visible())
	{
		line_box->UpdateField();
		line_box->UpdateSides();
	}

	if (sec_box->visible())
		sec_box->UpdateField();

	if (vert_box->visible())
		vert_box->UpdateField();
}


void UI_MainWin::UnselectPics()
{
	line_box->UnselectPics();
	 sec_box->UnselectPics();
	vert_box->UnselectPics();
}


void UI_MainWin::SetTitle(const char *wad_name, const char *map_name)
{
	static char title_buf[FL_PATH_MAX];

	if (wad_name)
	{
		wad_name = fl_filename_name(wad_name);
		sprintf(title_buf, "%s (%s)", wad_name, Level_name);
	}
	else
	{
		sprintf(title_buf, "Unsaved %s", Level_name);
	}

	strcat(title_buf, " - Eureka");

	copy_label(title_buf);

//--	info_bar->SetMap(Level_name);
}


void UI_MainWin::UpdateTitle(bool want_star)
{
	if (! label())
		return;

	bool got_star = (label()[0] == '*');

	if (want_star == got_star)
		return;

	static char title_buf[FL_PATH_MAX];

	if (got_star)
	{
		strcpy(title_buf, label() + 1);
	}
	else
	{
		title_buf[0] = '*';
		strcpy(title_buf + 1, label());
	}

	copy_label(title_buf);

//--	info_bar->SetMap(Level_name, want_star);
}


/* DISABLED, since it fails miserably on every platform

void UI_MainWin::ToggleFullscreen()
{
	if (last_w)
	{
		fullscreen_off(last_x, last_y, last_w, last_h);

		last_w = last_h = 0;
	}
	else
	{
		last_x = x(); last_y = y();
		last_w = w(); last_h = h();

		fullscreen();
	}
}
*/


void UI_MainWin::BrowsedItem(char kind, int number, const char *name, int e_state)
{
//	fprintf(stderr, "BrowsedItem: kind '%c' --> %d / \"%s\"\n", kind, number, name);

	switch (edit.mode)
	{
		case OBJ_LINEDEFS:
			if (kind == 'T')
			{
				line_box->SetTexture(name, e_state);
				return;
			}
			if (kind == 'L')
			{
				line_box->SetLineType(number);
				return;
			}
			break;

		case OBJ_SECTORS:
			if (kind == 'F')
			{
				sec_box->SetFlat(name, e_state);
				return;
			}
			if (kind == 'S')
			{
				sec_box->SetSectorType(number);
				return;
			}
			break;

		case OBJ_THINGS:
			if (kind == 'O')
			{
				thing_box->SetThingType(number);
				return;
			}
			break;

		case OBJ_VERTICES:
			// pass through for Default Props
			vert_box->BrowsedItem(kind, number, name, e_state);
			return;

		default:
			break;
	}

	fl_beep();
}


void UI_MainWin::Maximize()
{
#if defined(WIN32)
	HWND hWnd = fl_xid(pWindow);

	ShowWindow(hWnd, SW_MAXIMIZE);

#elif defined(__APPLE__)
	// FIXME : something like this... (AFAIK)
	//
	// Window handle = fl_xid(this);   // really an NSWindow *
	//
	// if (! [handle isZoomed])
	//     [handle zoom:nil];

#else  /* Linux + X11 */

	Window xid = fl_xid(this);

	Atom wm_state = XInternAtom(fl_display, "_NET_WM_STATE", False);
	Atom vmax = XInternAtom(fl_display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
	Atom hmax = XInternAtom(fl_display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);

	XEvent xev;
	memset(&xev, 0, sizeof(xev));

	xev.type = ClientMessage;
	xev.xclient.window = xid;
	xev.xclient.message_type = wm_state;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = 2;
	xev.xclient.data.l[1] = hmax;
	xev.xclient.data.l[2] = vmax;

	XSendEvent(fl_display, DefaultRootWindow(fl_display), False,
			   SubstructureNotifyMask, &xev);

	Delay(3);
#endif
}


void UI_MainWin::Delay(int steps)
{
	for (; steps > 0 ; steps--)
	{
		TimeDelay(100);

		Fl::check();
	}
}


//------------------------------------------------------------------------


UI_Escapable_Window::UI_Escapable_Window(int W, int H, const char *L) :
	Fl_Double_Window(W, H, L)
{ }


UI_Escapable_Window::~UI_Escapable_Window()
{ }


int UI_Escapable_Window::handle(int event)
{
	if (event == FL_KEYDOWN && Fl::event_key() == FL_Escape)
	{
		do_callback();
		return 1;
	}

	return Fl_Double_Window::handle(event);
}


//------------------------------------------------------------------------


// TODO: config item
#define MAX_LOG_LINES  600


UI_LogViewer * log_viewer;


UI_LogViewer::UI_LogViewer() :
	UI_Escapable_Window(550, 400, "Eureka Log Viewer")
{
	box(FL_NO_BOX);

	size_range(385, 200);

	int ey = h() - 65;

	browser = new Fl_Browser(0, 0, w(), ey);

	resizable(browser);

	{
		Fl_Group *o = new Fl_Group(0, ey, w(), h() - ey);
		o->box(FL_FLAT_BOX);
		o->color(fl_gray_ramp(4));
		
		int bx = w() - 110;
		{
			Fl_Button * but = new Fl_Button(bx, ey + 15, 80, 35, "OK");
			but->labelfont(1);
//!!!		but->callback(close_callback, this);
		}

		bx = bx - 110;
		{
			Fl_Button * but = new Fl_Button(bx, ey + 15, 80, 35, "Clear");
//!!!		but->callback(close_callback, this);
		}

		bx = bx - 110;
		{
			Fl_Button * but = new Fl_Button(bx, ey + 15, 80, 35, "Save...");
//!!!		but->callback(close_callback, this);
		}

		Fl_Group *resize_box = new Fl_Group(4, ey + 2, 180, h() - ey - 4);
		resize_box->box(FL_FLAT_BOX);
		resize_box->color(FL_RED, FL_RED);

		o->resizable(resize_box);

		o->end();
	}

	end();
}


UI_LogViewer::~UI_LogViewer()
{ }


void UI_LogViewer::Add(const char *line)
{
	browser->add(line);

	if (browser->size() > MAX_LOG_LINES)
		browser->remove(1);

	if (shown())
		browser->bottomline(browser->size());
}


void LogViewer_AddLine(const char *str)
{
	if (log_viewer)
		log_viewer->Add(str);
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
