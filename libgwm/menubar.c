/*
	Glidix GUI

	Copyright (c) 2014-2017, Madd Games.
	All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	* Redistributions of source code must retain the above copyright notice, this
	  list of conditions and the following disclaimer.
	
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	
	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <libgwm.h>
#include <libddi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define	MENUBAR_HEIGHT			20

typedef struct
{
	size_t				numMenus;
	GWMMenu**			menus;
	const char**			labels;
	int*				offsets;
	int*				widths;
	GWMWindow*			parent;
	DDISurface*			textSurface;
} GWMMenubarData;

static int menubarHandler(GWMEvent *ev, GWMWindow *menubar, void *context);

void gwmRedrawMenubar(GWMWindow *menubar, int i)
{
	static DDIColor black = {0x00, 0x00, 0x00, 0xFF};
	
	GWMMenubarData *data = (GWMMenubarData*) gwmGetData(menubar, menubarHandler);
	gwmClearWindow(menubar);
	DDISurface *canvas = gwmGetWindowCanvas(menubar);
	
	ddiFillRect(canvas, 0, MENUBAR_HEIGHT-1, canvas->width, 1, &black);
	
	if (i != -1)
	{
		ddiFillRect(canvas, data->offsets[i], 0, data->widths[i], MENUBAR_HEIGHT-1, GWM_COLOR_SELECTION);
	};
	
	ddiBlit(data->textSurface, 0, 0, canvas, 0, 0, canvas->width, canvas->height);
	gwmPostDirty(menubar);
};

static void menuClose(void *context)
{
	GWMWindow *menubar = (GWMWindow*) context;
	gwmRedrawMenubar(menubar, -1);
};

void gwmRenderMenubar(GWMWindow *menubar)
{
	static DDIColor transparent = {0, 0, 0, 0};
	
	GWMMenubarData *data = (GWMMenubarData*) gwmGetData(menubar, menubarHandler);
	if (data->textSurface != NULL)
	{
		ddiDeleteSurface(data->textSurface);
	};
	
	DDISurface *canvas = gwmGetWindowCanvas(menubar);
	data->textSurface = ddiCreateSurface(&canvas->format, canvas->width, canvas->height, NULL, 0);
	ddiFillRect(data->textSurface, 0, 0, canvas->width, canvas->height, &transparent);
	
	DDIPen *pen = gwmGetPen(menubar, 2, MENUBAR_HEIGHT-6, canvas->width, MENUBAR_HEIGHT-4);
	if (pen != NULL)
	{
		ddiSetPenWrap(pen, 0);
		
		size_t i;
		int currentOffset = 2;
		for (i=0; i<data->numMenus; i++)
		{
			data->offsets[i] = currentOffset;
			ddiWritePen(pen, data->labels[i]);
			ddiWritePen(pen, " ");
			
			int width, height;
			ddiGetPenSize(pen, &width, &height);
			
			data->widths[i] = width - currentOffset;
			currentOffset = width;
		};
		
		data->offsets[0] = 0;
		if (data->numMenus != 0) data->widths[0] += 2;

		ddiExecutePen2(pen, data->textSurface, DDI_POSITION_BASELINE);
		ddiDeletePen(pen);
	};
	
	gwmRedrawMenubar(menubar, -1);
};

static int menubarHandler(GWMEvent *ev, GWMWindow *menubar, void *context)
{
	GWMMenubarData *data = (GWMMenubarData*) gwmGetData(menubar, menubarHandler);
	size_t i;
	
	switch (ev->type)
	{
	case GWM_EVENT_RETHEME:
		gwmRenderMenubar(menubar);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_UP:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			for (i=0; i<data->numMenus; i++)
			{
				if ((data->offsets[i] <= ev->x) && ((data->offsets[i]+data->widths[i]) > ev->x))
				{
					gwmRedrawMenubar(menubar, (int)i);
					gwmOpenMenu(data->menus[i], menubar, data->offsets[i], MENUBAR_HEIGHT-1);
					break;
				};
			};
		};
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static void menubarSize(GWMWindow *menubar, int *width, int *height)
{
	*width = 100;
	*height = MENUBAR_HEIGHT;
};

static void menubarPosition(GWMWindow *menubar, int x, int y, int width, int height)
{
	y += (height - MENUBAR_HEIGHT) / 2;
	gwmMoveWindow(menubar, x, y);
	gwmResizeWindow(menubar, width, height);
	gwmRenderMenubar(menubar);
};

GWMWindow *gwmCreateMenubar(GWMWindow *parent)
{
	DDISurface *canvas = gwmGetWindowCanvas(parent);
	
	GWMWindow *menubar = gwmCreateWindow(parent, "GWMMenubar", 0, 0, canvas->width, MENUBAR_HEIGHT, 0);
	GWMMenubarData *data = (GWMMenubarData*) malloc(sizeof(GWMMenubarData));
	data->numMenus = 0;
	data->menus = NULL;
	data->offsets = NULL;
	data->widths = NULL;
	data->labels = NULL;
	data->parent = parent;
	data->textSurface = NULL;

	gwmPushEventHandler(menubar, menubarHandler, data);
	
	menubar->getMinSize = menubar->getPrefSize = menubarSize;
	menubar->position = menubarPosition;
	
	return menubar;
};

void gwmDestroyMenubar(GWMWindow *menubar)
{
	GWMMenubarData *data = (GWMMenubarData*) gwmGetData(menubar, menubarHandler);
	free(data->menus);
	free(data->offsets);
	free(data->widths);
	free(data->labels);
	ddiDeleteSurface(data->textSurface);
	
	free(data);
	gwmDestroyWindow(menubar);
};

GWMWindow* gwmNewMenubar(GWMWindow *parent)
{
	return gwmCreateMenubar(parent);
};

void gwmMenubarAdjust(GWMWindow *menubar)
{
	GWMMenubarData *data = (GWMMenubarData*) gwmGetData(menubar, menubarHandler);
	DDISurface *parentCanvas = gwmGetWindowCanvas(data->parent);
	gwmResizeWindow(menubar, parentCanvas->width, MENUBAR_HEIGHT);
	gwmRenderMenubar(menubar);
};

void gwmMenubarAdd(GWMWindow *menubar, const char *label, GWMMenu *menu)
{
	GWMMenubarData *data = (GWMMenubarData*) gwmGetData(menubar, menubarHandler);

	size_t index = data->numMenus++;
	data->menus = realloc(data->menus, sizeof(GWMMenu*) * data->numMenus);
	data->labels = realloc(data->labels, sizeof(const char*) * data->numMenus);
	data->offsets = realloc(data->offsets, sizeof(int) * data->numMenus);
	data->widths = realloc(data->widths, sizeof(int) * data->numMenus);
	
	data->menus[index] = menu;
	data->labels[index] = strdup(label);
	
	gwmMenuSetCloseCallback(menu, menuClose, menubar);
};
