/*
	Glidix GUI

	Copyright (c) 2014-2017, Madd Games.
	All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	* Redistributions of source code must retain the above copyright notice, this
	  list of conditions and the following disclaimer.
	
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentationx
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

#include <assert.h>
#include <libgwm.h>
#include <libddi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define	RADIO_WIDTH				20
#define	RADIO_HEIGHT				20

enum
{
	RADIO_OFF,
	RADIO_ON
};

enum
{
	RADIO_NORMAL,
	RADIO_HOVER,
	RADIO_PRESSED,
	RADIO_DISABLED
};

typedef struct GWMRadioData_
{
	GWMWindow*				win;
	GWMRadioGroup*				group;
	int					value;
	int					flags;
	int					state;
	int					minWidth;
	int					symbol;
	struct GWMRadioData_*			prev;
	struct GWMRadioData_*			next;
	char*					text;
} GWMRadioData;

static int gwmRadioHandler(GWMEvent *ev, GWMWindow *radio, void *context);

GWMRadioGroup* gwmCreateRadioGroup(int value)
{
	GWMRadioGroup *group = (GWMRadioGroup*) malloc(sizeof(GWMRadioGroup));
	group->value = value;
	group->first = NULL;
	group->last = NULL;
	return group;
};

void gwmDestroyRadioGroup(GWMRadioGroup *group)
{
	assert(group->first == NULL);
	free(group);
};

static void gwmRedrawRadio(GWMWindow *radio)
{
	GWMRadioData *data = (GWMRadioData*) gwmGetData(radio, gwmRadioHandler);
	
	int iy = data->state;
	if (data->flags & GWM_RADIO_DISABLED)
	{
		iy = RADIO_DISABLED;
	};
	
	int ix = (data->value == data->group->value);
	
	DDISurface *canvas = gwmGetWindowCanvas(radio);

	static DDIColor transparent = {0, 0, 0, 0};
	ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &transparent);
	
	DDISurface *imgRadio = gwmGetThemeSurface("gwm.toolkit.radio");
	ddiBlit(imgRadio, RADIO_WIDTH*ix, RADIO_HEIGHT*iy, canvas, 0, 0, RADIO_WIDTH, RADIO_HEIGHT);

	DDIPen *pen = gwmGetPen(radio, 0, 0, canvas->width, canvas->height);
	ddiSetPenWrap(pen, 0);
	ddiWritePen(pen, data->text);
	
	int txtWidth, txtHeight;
	ddiGetPenSize(pen, &txtWidth, &txtHeight);
	ddiSetPenPosition(pen, 22, 10-(txtHeight/2));
	ddiExecutePen(pen, canvas);
	ddiDeletePen(pen);
	
	data->minWidth = txtWidth + 22;

	gwmPostDirty(radio);
};

static void gwmRedrawRadioGroup(GWMRadioGroup *group)
{
	GWMRadioData *data;
	for (data=group->first; data!=NULL; data=data->next)
	{
		gwmRedrawRadio(data->win);
	};
};

int gwmGetRadioGroupValue(GWMRadioGroup *group)
{
	return group->value;
};

void gwmSetRadioGroupValue(GWMRadioGroup *group, int value)
{
	group->value = value;
	gwmRedrawRadioGroup(group);
};

static int gwmRadioHandler(GWMEvent *ev, GWMWindow *radio, void *context)
{
	GWMRadioData *data = (GWMRadioData*) gwmGetData(radio, gwmRadioHandler);
	
	switch (ev->type)
	{
	case GWM_EVENT_ENTER:
		data->state = RADIO_HOVER;
		gwmRedrawRadio(radio);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_FOCUS_OUT:
	case GWM_EVENT_LEAVE:
		data->state = RADIO_NORMAL;
		gwmRedrawRadio(radio);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_DOWN:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			data->state = RADIO_PRESSED;
			gwmRedrawRadio(radio);
		};
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_UP:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			if (data->state == RADIO_PRESSED)
			{
				data->state = RADIO_HOVER;
				
				if ((data->flags & GWM_RADIO_DISABLED) == 0)
				{
					GWMCommandEvent cmdev;
					memset(&cmdev, 0, sizeof(GWMCommandEvent));
					cmdev.header.type = GWM_EVENT_COMMAND;
					cmdev.symbol = data->symbol;
					
					if (gwmPostEvent((GWMEvent*) &cmdev, radio) == GWM_EVSTATUS_DEFAULT)
					{
						data->group->value = data->value;
						gwmRedrawRadioGroup(data->group);
						
						memset(&cmdev, 0, sizeof(GWMCommandEvent));
						cmdev.header.type = GWM_EVENT_TOGGLED;
						cmdev.symbol = data->symbol;
						
						gwmPostEvent((GWMEvent*) &cmdev, radio);
					};
				};
			};
		};
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_RETHEME:
		gwmRedrawRadio(radio);
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static void radioSize(GWMWindow *radio, int *width, int *height)
{
	GWMRadioData *data = (GWMRadioData*) gwmGetData(radio, gwmRadioHandler);
	*width = data->minWidth;
	*height = 20;
};

static void radioPosition(GWMWindow *radio, int x, int y, int width, int height)
{
	y += (height - 20) / 2;
	gwmMoveWindow(radio, x, y);
	gwmResizeWindow(radio, width, 20);
	gwmRedrawRadio(radio);
};

GWMWindow* gwmCreateRadioButton(GWMWindow *parent, int x, int y, GWMRadioGroup *group, int value, int flags)
{
	GWMWindow *radio = gwmCreateWindow(parent, "GWMRadioButton", x, y, RADIO_WIDTH, RADIO_HEIGHT, 0);
	if (radio == NULL) return NULL;
	
	GWMRadioData *data = (GWMRadioData*) malloc(sizeof(GWMRadioData));
	data->win = radio;
	data->group = group;
	data->value = value;
	data->flags = flags;
	data->state = RADIO_NORMAL;
	data->prev = group->last;
	data->next = NULL;
	data->text = strdup("");
	data->minWidth = 20;
	data->symbol = 0;
	
	radio->getMinSize = radio->getPrefSize = radioSize;
	radio->position = radioPosition;
	
	if (group->last == NULL)
	{
		group->first = group->last = data;
	}
	else
	{
		group->last->next = data;
		group->last = data;
	};
	
	gwmPushEventHandler(radio, gwmRadioHandler, data);
	gwmRedrawRadio(radio);
	return radio;
};

GWMWindow* gwmNewRadioButton(GWMWindow *parent, GWMRadioGroup *group)
{
	return gwmCreateRadioButton(parent, 0, 0, group, 0, 0);
};

void gwmSetRadioButtonValue(GWMWindow *radio, int value)
{
	GWMRadioData *data = (GWMRadioData*) gwmGetData(radio, gwmRadioHandler);
	data->value = value;
	gwmRedrawRadio(radio);
};

void gwmSetRadioButtonLabel(GWMWindow *radio, const char *text)
{
	GWMRadioData *data = (GWMRadioData*) gwmGetData(radio, gwmRadioHandler);
	free(data->text);
	data->text = strdup(text);
	gwmRedrawRadio(radio);
};

void gwmSetRadioButtonSymbol(GWMWindow *radio, int symbol)
{
	GWMRadioData *data = (GWMRadioData*) gwmGetData(radio, gwmRadioHandler);
	data->symbol = symbol;
};

void gwmDestroyRadioButton(GWMWindow *radio)
{
	GWMRadioData *data = (GWMRadioData*) gwmGetData(radio, gwmRadioHandler);
	
	if (data->group->first == data)
	{
		data->group->first = data->next;
	};
	
	if (data->group->last == data)
	{
		data->group->last = data->prev;
	};
	
	if (data->prev != NULL)
	{
		data->prev->next = data->next;
	};
	
	if (data->next != NULL)
	{
		data->next->prev = data->prev;
	};
	
	free(data);
	gwmDestroyWindow(radio);
};
