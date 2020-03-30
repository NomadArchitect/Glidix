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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define	TEXTFIELD_MIN_WIDTH	150

#define	TXT_STYLE_SET_FG	1
#define	TXT_STYLE_SET_BG	2

#define	TXT_TEMPL_WIDTH		17
#define	TXT_HEIGHT		30

typedef struct TextStyle_
{
	struct TextStyle_*	prev;
	struct TextStyle_*	next;
	uint64_t		offset;		// in unicode characters
	int			type;
	
	union
	{
		DDIColor	color;
	};
} TextStyle;

typedef struct
{
	char			*text;
	off_t			cursorPos;
	int			focused;
	int			flags;
	
	/**
	 * Selection range.
	 */
	off_t			selectStart;
	off_t			selectEnd;
	
	/**
	 * If the mouse button isn't down, this is -1. Otherwise, it's the position
	 * that was initally clicked. Used to define the selection range.
	 */
	off_t			clickPos;
	
	/**
	 * Pen used to draw the text.
	 */
	DDIPen*			pen;
	
	/**
	 * The right-click menu.
	 */
	GWMMenu*		menu;
	
	/**
	 * Icon or NULL.
	 */
	DDISurface*		icon;
	
	/**
	 * Placeholder text if empty (or NULL for no placeholder).
	 */
	char*			placeholder;
	
	/**
	 * Whether the text should wrap (false by default).
	 */
	int			wrap;
	
	/**
	 * Text alignment (left by default).
	 */
	int			align;
	
	/**
	 * Font.
	 */
	DDIFont*		font;
	
	/**
	 * Scrollbars.
	 */
	GWMScrollbar*		sbarX;
	GWMScrollbar*		sbarY;
	
	/**
	 * List of text styles.
	 */
	TextStyle*		styles;
} GWMTextFieldData;

static DDIFont *fntPlaceHolder;

int gwmTextFieldHandler(GWMEvent *ev, GWMWindow *field, void *context);

void gwmRedrawTextField(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	DDISurface *canvas = gwmGetWindowCanvas(field);
	
	static DDIColor transparent = {0, 0, 0, 0};
	
	int penX = 3;
	if (data->icon != NULL)
	{
		penX = 20;
	};
	
	int penY = 0;
	
	if (data->flags & GWM_TXT_MULTILINE)
	{
		DDIColor *color = GWM_COLOR_FAINT;
		if (data->focused)
		{
			color = GWM_COLOR_SELECTION;
		};

		if (data->flags & GWM_TXT_DISABLED)
		{
			color = &transparent;
		};
		
		static DDIColor disabledBackground = {0x00, 0x00, 0x00, 0x00};
		DDIColor *background = GWM_COLOR_EDITOR;
		if (data->flags & GWM_TXT_DISABLED)
		{
			background = &disabledBackground;
		};
		
		ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, color);
		ddiFillRect(canvas, 1, 1, canvas->width-2, canvas->height-2, background);
	}
	else
	{
		ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &transparent);
		
		int whichImg = 0;
		if (data->focused)
		{
			whichImg = 1;
		};
		
		if (data->flags & GWM_TXT_DISABLED)
		{
			whichImg = 2;
		};
		
		// which horizontal image we use depends on stripping: starting with bit 4 of the
		// flags, we have 2 bits indicating this, and they can be interpreted as an index
		int templateOffsetX = 17 * ((data->flags >> 4) & 0x3);
		
		DDISurface *imgTextField = gwmGetThemeSurface("gwm.toolkit.textfield");

		DDISurface *temp = ddiCreateSurface(&canvas->format, TXT_TEMPL_WIDTH, TXT_HEIGHT, NULL, 0);
		assert(temp != NULL);
		ddiOverlay(imgTextField, templateOffsetX, TXT_HEIGHT*whichImg, temp, 0, 0, TXT_TEMPL_WIDTH, TXT_HEIGHT);
		DDISurface *scaled = ddiScale(temp, canvas->width, canvas->height, DDI_SCALE_BORDERED_GRADIENT);
		assert(scaled != NULL);
		ddiBlit(scaled, 0, 0, canvas, 0, 0, canvas->width, canvas->height);
		ddiDeleteSurface(scaled);
		ddiDeleteSurface(temp);
		
		penX += 2;
		penY = 6;
	};

	if (data->icon != NULL)
	{
		int iconX = 2;
		if ((data->flags & GWM_TXT_MULTILINE) == 0)
		{
			iconX = 5;
		};
		
		ddiBlit(data->icon, 0, 0, canvas, iconX, (TXT_HEIGHT-16)/2, 16, 16);
	};
	
	if (data->pen != NULL) ddiDeletePen(data->pen);
	
	data->pen = ddiCreatePen(&canvas->format, data->font, penX, penY, canvas->width-penX-10, canvas->height-13-penY, 0, 0, NULL);
	if (data->pen != NULL)
	{
		ddiSetPenWrap(data->pen, data->wrap);
		ddiSetPenAlignment(data->pen, data->align);
		if (data->flags & GWM_TXT_MASKED) ddiPenSetMask(data->pen, 1);
		if (data->focused) ddiSetPenCursor(data->pen, data->cursorPos);

		char *buffer = (char*) malloc(strlen(data->text)+1);
		const char *scan = data->text;
		size_t currentPos = 0;
		size_t totalChars = ddiCountUTF8(data->text);
		TextStyle *style = data->styles;
		
		while (*scan != 0)
		{
			// process all current styles
			while (style != NULL && style->offset == currentPos)
			{
				switch (style->type)
				{
				case TXT_STYLE_SET_FG:
					ddiSetPenColor(data->pen, &style->color);
					break;
				case TXT_STYLE_SET_BG:
					ddiSetPenBackground(data->pen, &style->color);
					break;
				};
				
				style = style->next;
			};
			
			// process selection
			if (currentPos == data->selectStart && data->selectStart != data->selectEnd)
			{
				ddiSetPenBackground(data->pen, GWM_COLOR_SELECTION);
			};
			
			if (currentPos == data->selectEnd && data->selectStart != data->selectEnd)
			{
				// TODO: this should actually restore the correct style!
				ddiSetPenBackground(data->pen, &transparent);
			};
			
			// figure out how far we're going
			size_t nextCount = totalChars - currentPos;
			if (style != NULL) nextCount = style->offset - currentPos;
			if (data->selectStart != data->selectEnd && data->selectStart > currentPos
				&& (data->selectStart - currentPos) < nextCount) nextCount = data->selectStart - currentPos;
			if (data->selectStart != data->selectEnd && data->selectEnd > currentPos
				&& (data->selectEnd - currentPos) < nextCount) nextCount = data->selectEnd - currentPos;
			
			// copy that into the buffer
			const char *endpos = ddiGetOffsetUTF8(scan, nextCount);
			size_t byteSize = endpos - scan;
			memcpy(buffer, scan, byteSize);
			buffer[byteSize] = 0;
			
			// write it
			ddiWritePen(data->pen, buffer);
			
			// advance
			currentPos += nextCount;
			scan = endpos;
		};
		
		if ((data->flags & GWM_TXT_MULTILINE) == 0)
		{
			gwmHide(data->sbarX);
			gwmHide(data->sbarY);
		}
		else
		{
			int penWidth, penHeight;
			ddiGetPenSize(data->pen, &penWidth, &penHeight);
			
			int scrollX=0, scrollY=0;
			
			if (penWidth <= canvas->width)
			{
				gwmHide(data->sbarX);
			}
			else
			{
				float len = (float) (canvas->width-13) / (float) penWidth;
				gwmSetScrollbarLength(data->sbarX, len);
				
				scrollX = penWidth * gwmGetScrollbarPosition(data->sbarX);
				gwmShow(data->sbarX);
			};
			
			if (penHeight <= canvas->height)
			{
				gwmHide(data->sbarY);
			}
			else
			{
				float len = (float) (canvas->height-13) / (float) penHeight;
				gwmSetScrollbarLength(data->sbarY, len);
				
				scrollY = penHeight * gwmGetScrollbarPosition(data->sbarY);
				gwmShow(data->sbarY);
			};
			
			ddiSetPenPosition(data->pen, 3-scrollX, 3-scrollY);
		};
		
		ddiExecutePen(data->pen, canvas);
	};
	
	if (data->text[0] == 0 && data->placeholder != NULL)
	{
		if (fntPlaceHolder == NULL)
		{
			const char *error;
			fntPlaceHolder = ddiLoadFont("OpenSans", 13, DDI_STYLE_ITALIC, &error);
			if (fntPlaceHolder == NULL)
			{
				fprintf(stderr, "Failed to load OpenSans 13 italic: %s\n", error);
				abort();
			};
		};
		
		static DDIColor colPlaceHolder = {0x77, 0x77, 0x77, 0xFF};
		DDIPen *pen = ddiCreatePen(&canvas->format, fntPlaceHolder, penX, penY, canvas->width-penX, canvas->height-3, 0, 0, NULL);
		ddiSetPenAlignment(pen, data->align);
		ddiSetPenColor(pen, &colPlaceHolder);
		ddiSetPenWrap(pen, data->wrap);
		ddiWritePen(pen, data->placeholder);
		ddiExecutePen(pen, canvas);
		ddiDeletePen(pen);
	};
	
	gwmPostDirty(field);
};

static void gwmTextFieldDeleteSelection(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	
	char *newBuffer = (char*) malloc(strlen(data->text)+1);
	newBuffer[0] = 0;
	
	const char *scan = data->text;
	
	// copy the part before selection
	size_t count = data->selectStart;
	while (count--)
	{
		long codepoint = ddiReadUTF8(&scan);
		if (codepoint == 0) break;
		ddiWriteUTF8(&newBuffer[strlen(newBuffer)], codepoint);
	};
	
	// skip selection
	count = data->selectEnd - data->selectStart;
	while (count--)
	{
		ddiReadUTF8(&scan);
	};
	
	// write the part after selection
	while (1)
	{
		long codepoint = ddiReadUTF8(&scan);
		if (codepoint == 0) break;
		ddiWriteUTF8(&newBuffer[strlen(newBuffer)], codepoint);
	};
	
	// process styles
	size_t delta = data->selectEnd - data->selectStart;
	TextStyle *style = data->styles;
	while (style != NULL)
	{
		if (style->offset > data->selectStart)
		{
			if (style->offset < data->selectEnd)
			{
				// it's within the selection, delete it
				if (style->prev != NULL) style->prev->next = style->next;
				if (style->next != NULL) style->next->prev = style->prev;
				if (data->styles == style) data->styles = style->next;
				
				TextStyle *next = style->next;
				free(style);
				style = next;
				continue;
			}
			else
			{
				// push back
				style->offset -= delta;
			};
		};
		
		style = style->next;
	};
	
	// update
	data->cursorPos = data->selectStart;
	data->selectStart = data->selectEnd = 0;
	free(data->text);
	data->text = newBuffer;
};

void gwmTextFieldBackspace(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	if (data->selectStart != data->selectEnd)
	{
		gwmTextFieldDeleteSelection(field);
		gwmPostUpdate(field);
		return;
	};
	
	if (data->cursorPos == 0) return;

	char *newBuffer = (char*) malloc(strlen(data->text) + 1);
	newBuffer[0] = 0;
	
	const char *scan = data->text;
	
	// copy the data before the cursor except the last one
	// (the case where cursorPos == 0 is already handled above)
	size_t count = data->cursorPos - 1;
	while (count--)
	{
		long codepoint = ddiReadUTF8(&scan);
		if (codepoint == 0) break;
		ddiWriteUTF8(&newBuffer[strlen(newBuffer)], codepoint);
	};
	
	// discard one character
	ddiReadUTF8(&scan);
	
	// write out the rest
	while (1)
	{
		long codepoint = ddiReadUTF8(&scan);
		if (codepoint == 0) break;
		ddiWriteUTF8(&newBuffer[strlen(newBuffer)], codepoint);
	};
	
	free(data->text);
	data->text = newBuffer;
	data->cursorPos--;
	
	// all styles at the new cursor position must be deleted, and all styles after that must be pushed back
	TextStyle *style = data->styles;
	while (style != NULL)
	{
		if (style->offset > data->cursorPos)
		{
			// push back
			style->offset--;
		};
		
		style = style->next;
	};

	GWMCommandEvent cmdev;
	memset(&cmdev, 0, sizeof(GWMCommandEvent));
	cmdev.header.type = GWM_EVENT_VALUE_CHANGED;
	gwmPostEvent((GWMEvent*) &cmdev, field);

	gwmPostUpdate(field);
};

void gwmTextFieldInsert(GWMWindow *field, const char *str)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	if (data->selectStart != data->selectEnd)
	{
		gwmTextFieldDeleteSelection(field);
	};
	
	char *newBuffer = (char*) malloc(strlen(data->text) + strlen(str) + 1);
	newBuffer[0] = 0;
	
	// push all styles after the cursor forward
	size_t textCount = ddiCountUTF8(str);
	TextStyle *style;
	for (style=data->styles; style!=NULL; style=style->next)
	{
		if (style->offset >= data->cursorPos)
		{
			style->offset += textCount;
		};
	};
	
	const char *scan = data->text;
	
	// copy up to the cursor first
	size_t count = data->cursorPos;
	while (count--)
	{
		long codepoint = ddiReadUTF8(&scan);
		if (codepoint == 0) break;
		ddiWriteUTF8(&newBuffer[strlen(newBuffer)], codepoint);
	};
	
	// now copy the string, incrementing the cursor as necessary
	while (1)
	{
		long codepoint = ddiReadUTF8(&str);
		if (codepoint == 0) break;
		ddiWriteUTF8(&newBuffer[strlen(newBuffer)], codepoint);
		data->cursorPos++;
	};
	
	// finish off
	strcat(newBuffer, scan);
	
	free(data->text);
	data->text = newBuffer;

	GWMCommandEvent cmdev;
	memset(&cmdev, 0, sizeof(GWMCommandEvent));
	cmdev.header.type = GWM_EVENT_VALUE_CHANGED;
	gwmPostEvent((GWMEvent*) &cmdev, field);

	gwmPostUpdate(field);
};

void gwmTextFieldSelectWord(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	
	const char *scan = data->text;
	int lastClass = -1;
	off_t lastPos;
	
	// read up to the cursor position, classifying characters on the way, remembering each position where
	// the classification changed.
	size_t count = data->cursorPos;
	off_t curPos = 0;
	while (count--)
	{
		long codepoint = ddiReadUTF8(&scan);
		int cls = gwmClassifyChar(codepoint);
		
		if (cls != lastClass)
		{
			lastClass = cls;
			lastPos = curPos;
		};
		
		curPos++;
	};
	
	// figure out where to stop
	off_t endPos = curPos;
	while (1)
	{
		long codepoint = ddiReadUTF8(&scan);
		if (codepoint == 0) break;
		int cls = gwmClassifyChar(codepoint);
		
		if (lastClass == -1)
		{
			lastClass = cls;
			lastPos = endPos;
		};
		
		if (cls != lastClass) break;
		endPos++;
	};
	
	// results
	if (lastClass == -1)
	{
		data->selectStart = data->selectEnd = 0;
	}
	else
	{
		data->selectStart = lastPos;
		data->selectEnd = endPos;
	};
	
	gwmPostUpdate(field);
};

int txtPaste(void *context)
{
	GWMWindow *field = (GWMWindow*) context;
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	
	if (data->flags & GWM_TXT_DISABLED) return 0;
	
	size_t size;
	char *text = gwmClipboardGetText(&size);
	if (text != NULL)
	{
		gwmTextFieldInsert(field, text);
		free(text);
	};

	gwmFocus(field);
	return 0;
};

int txtCopy(void *context)
{
	GWMWindow *field = (GWMWindow*) context;
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	
	if (data->selectStart != data->selectEnd)
	{
		char *temp = (char*) malloc(strlen(data->text) + 1);
		temp[0] = 0;
		
		const char *scan = data->text;
		
		// skip the text before selection
		size_t count = data->selectStart;
		while (count--)
		{
			ddiReadUTF8(&scan);
		};
		
		// now copy selection into buffer
		count = data->selectEnd - data->selectStart;
		while (count--)
		{
			long codepoint = ddiReadUTF8(&scan);
			if (codepoint == 0) break;
			ddiWriteUTF8(&temp[strlen(temp)], codepoint);
		};
		
		// store in the clipboard
		gwmClipboardPutText(temp, strlen(temp));
		free(temp);
	};
	
	gwmFocus(field);
	return 0;
};

int txtCut(void *context)
{
	GWMWindow *field = (GWMWindow*) context;
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	
	if (data->flags & GWM_TXT_DISABLED) return txtCopy(context);

	if (data->selectStart != data->selectEnd)
	{
		char *temp = (char*) malloc(strlen(data->text) + 1);
		temp[0] = 0;
		
		const char *scan = data->text;
		
		// skip the text before selection
		size_t count = data->selectStart;
		while (count--)
		{
			ddiReadUTF8(&scan);
		};
		
		// now copy selection into buffer
		count = data->selectEnd - data->selectStart;
		while (count--)
		{
			long codepoint = ddiReadUTF8(&scan);
			if (codepoint == 0) break;
			ddiWriteUTF8(&temp[strlen(temp)], codepoint);
		};
		
		// store in the clipboard
		gwmClipboardPutText(temp, strlen(temp));
		free(temp);
	};
	
	gwmTextFieldDeleteSelection(field);
	gwmPostUpdate(field);
	gwmFocus(field);

	return 0;
};

int txtSelectAll(void *context)
{
	GWMWindow *field = (GWMWindow*) context;
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	data->selectStart = 0;
	data->selectEnd = ddiCountUTF8(data->text);
	data->cursorPos = data->selectEnd;
	gwmPostUpdate(field);
	gwmFocus(field);
	return 0;
};

int gwmTextFieldHandler(GWMEvent *ev, GWMWindow *field, void *context)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	off_t newCursorPos;
	int disabled = data->flags & GWM_TXT_DISABLED;
	char buf[9];
	
	GWMCommandEvent cmdev;
	
	switch (ev->type)
	{
	case GWM_EVENT_UPDATE:
		gwmRedrawTextField(field);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_FOCUS_IN:
		data->focused = 1;
		gwmPostUpdate(field);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_FOCUS_OUT:
		data->focused = 0;
		gwmPostUpdate(field);
		memset(&cmdev, 0, sizeof(GWMCommandEvent));
		cmdev.header.type = GWM_EVENT_EDIT_END;
		return gwmPostEvent((GWMEvent*) &cmdev, field);
	case GWM_EVENT_DOUBLECLICK:
		gwmTextFieldSelectWord(field);
		gwmPostUpdate(field);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_DOWN:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			newCursorPos = ddiPenCoordsToPos(data->pen, ev->x, ev->y);
			data->cursorPos = newCursorPos;
			data->clickPos = data->cursorPos;
			data->selectStart = 0;
			data->selectEnd = 0;
			gwmPostUpdate(field);
		}
		else if (ev->keycode == GWM_KC_LEFT)
		{
			data->selectStart = data->selectEnd = 0;
			if (data->cursorPos != 0)
			{
				data->cursorPos--;
				gwmPostUpdate(field);
			};
			
			return GWM_EVSTATUS_OK;
		}
		else if (ev->keycode == GWM_KC_RIGHT)
		{
			data->selectStart = data->selectEnd = 0;
			if (data->cursorPos != (off_t)ddiCountUTF8(data->text))
			{
				data->cursorPos++;
				gwmPostUpdate(field);
			};
			
			return GWM_EVSTATUS_OK;
		}
		else if (ev->keymod & GWM_KM_CTRL)
		{
			if (ev->keycode == 'c')
			{
				txtCopy(field);
			}
			else if ((ev->keycode == 'x') && (!disabled))
			{
				txtCut(field);
			}
			else if ((ev->keycode == 'v') && (!disabled))
			{
				txtPaste(field);
			}
			else if (ev->keycode == 'a')
			{
				txtSelectAll(field);
			};
			
			return GWM_EVSTATUS_OK;
		}
		else if (ev->keycode == GWM_KC_TAB && (data->flags & GWM_TXT_MULTILINE) == 0)
		{
			return GWM_EVSTATUS_CONT;
		}
		else if (ev->keycode == '\r')
		{
			if (data->flags & GWM_TXT_MULTILINE)
			{
				if (!disabled)
				{
					ddiWriteUTF8(buf, '\n');
					gwmTextFieldInsert(field, buf);
				};
			}
			else
			{
				GWMCommandEvent cmdev;
				memset(&cmdev, 0, sizeof(GWMCommandEvent));
				cmdev.header.type = GWM_EVENT_COMMAND;
				cmdev.symbol = GWM_SYM_OK;
				return gwmPostEvent((GWMEvent*) &cmdev, field);
			};
			
			return GWM_EVSTATUS_OK;
		}
		else if ((ev->keycode == '\b') && (!disabled))
		{
			gwmTextFieldBackspace(field);
			return GWM_EVSTATUS_OK;
		}
		else if ((ev->keychar != 0) && (!disabled))
		{
			//sprintf(buf, "%c", (char)ev->keychar);
			ddiWriteUTF8(buf, ev->keychar);
			gwmTextFieldInsert(field, buf);
			return GWM_EVSTATUS_OK;
		};
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_UP:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			data->clickPos = -1;
		}
		else if (ev->keycode == GWM_KC_MOUSE_RIGHT)
		{
			gwmOpenMenu(data->menu, field, ev->x, ev->y);
		};
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_MOTION:
		if (data->clickPos != -1)
		{
			newCursorPos = ddiPenCoordsToPos(data->pen, ev->x, ev->y);
			data->cursorPos = newCursorPos;
			if (newCursorPos < data->clickPos)
			{
				data->selectStart = newCursorPos;
				data->selectEnd = data->clickPos;
			}
			else
			{
				data->selectStart = data->clickPos;
				data->selectEnd = newCursorPos;
			};
			gwmPostUpdate(field);
		};
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_VALUE_CHANGED:
		if (ev->win == data->sbarX->id || ev->win == data->sbarY->id)
		{
			gwmPostUpdate(field);
			return GWM_EVSTATUS_OK;		/* do not forward to parent */
		};
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_RETHEME:
		gwmPostUpdate(field);
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static void txtGetSize(GWMWindow *field, int *width, int *height)
{
	*width = TEXTFIELD_MIN_WIDTH;
	*height = TXT_HEIGHT;
};

static void txtPosition(GWMWindow *field, int x, int y, int width, int height)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	
	gwmMoveWindow(field, x, y);
	gwmResizeWindow(field, width, height);
	
	data->sbarX->position(data->sbarX, 0, height-10, width-10, 10);
	data->sbarY->position(data->sbarY, width-10, 0, 10, height-10);
	
	gwmPostUpdate(field);
};

GWMWindow *gwmCreateTextField(GWMWindow *parent, const char *text, int x, int y, int width, int flags)
{
	GWMWindow *field = gwmCreateWindow(parent, "GWMTextField", x, y, width, 20, 0);
	if (field == NULL) return NULL;
	
	gwmAcceptTabs(field);
	
	GWMTextFieldData *data = (GWMTextFieldData*) malloc(sizeof(GWMTextFieldData));
	
	data->text = strdup(text);
	data->cursorPos = 0;
	data->focused = 0;
	data->flags = flags;
	
	data->selectStart = data->selectEnd = 0;
	data->clickPos = -1;
	data->pen = NULL;
	data->icon = NULL;
	data->placeholder = NULL;
	data->wrap = 0;
	data->align = DDI_ALIGN_LEFT;
	data->font = gwmGetDefaultFont();
	data->styles = NULL;
	
	field->getMinSize = field->getPrefSize = txtGetSize;
	field->position = txtPosition;
	
	data->menu = gwmCreateMenu();
	gwmMenuAddEntry(data->menu, "Cut", txtCut, field);
	gwmMenuAddEntry(data->menu, "Copy", txtCopy, field);
	gwmMenuAddEntry(data->menu, "Paste", txtPaste, field);
	gwmMenuAddSeparator(data->menu);
	gwmMenuAddEntry(data->menu, "Select all", txtSelectAll, field);
	
	data->sbarX = gwmNewScrollbar(field);
	data->sbarY = gwmNewScrollbar(field);
	gwmSetScrollbarFlags(data->sbarX, GWM_SCROLLBAR_HORIZ);

	gwmHide(data->sbarX);
	gwmHide(data->sbarY);
	
	gwmPushEventHandler(field, gwmTextFieldHandler, data);
	gwmPostUpdate(field);
	gwmSetWindowCursor(field, GWM_CURSOR_TEXT);
	
	return field;
};

GWMWindow* gwmNewTextField(GWMWindow *parent)
{
	return gwmCreateTextField(parent, "", 0, 0, 0, 0);
};

void gwmDestroyTextField(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	free(data->text);
	free(data->placeholder);
	free(data);
	gwmDestroyScrollbar(data->sbarX);
	gwmDestroyScrollbar(data->sbarY);
	gwmDestroyMenu(data->menu);
	gwmDestroyWindow(field);
};

size_t gwmGetTextFieldSize(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	return strlen(data->text);
};

const char* gwmReadTextField(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	return data->text;
};

void gwmWriteTextField(GWMWindow *field, const char *newText)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	free(data->text);
	
	data->text = strdup(newText);
	size_t count = 0;
	while (ddiReadUTF8(&newText) != 0)
	{
		count++;
	};
	
	data->selectStart = data->selectEnd = data->cursorPos = count;

	GWMCommandEvent cmdev;
	memset(&cmdev, 0, sizeof(GWMCommandEvent));
	cmdev.header.type = GWM_EVENT_VALUE_CHANGED;
	gwmPostEvent((GWMEvent*) &cmdev, field);

	gwmPostUpdate(field);
};

void gwmTextFieldSelectAll(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	data->selectStart = 0;
	data->selectEnd = ddiCountUTF8(data->text);
	data->cursorPos = data->selectEnd;
	gwmPostUpdate(field);
};

void gwmSetTextFieldFlags(GWMWindow *field, int flags)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	data->flags = flags;
	gwmPostUpdate(field);
};

void gwmSetTextFieldIcon(GWMTextField *field, DDISurface *icon)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	data->icon = icon;
	gwmPostUpdate(field);
};

void gwmSetTextFieldPlaceholder(GWMTextField *field, const char *placeholder)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	free(data->placeholder);
	data->placeholder = strdup(placeholder);
	gwmPostUpdate(field);
};

void gwmSetTextFieldWrap(GWMTextField *field, GWMbool wrap)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	data->wrap = wrap;
	gwmPostUpdate(field);
};

void gwmSetTextFieldAlignment(GWMTextField *field, int align)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	data->align = align;
	gwmPostUpdate(field);
};

void gwmSetTextFieldFont(GWMTextField *field, DDIFont *font)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	data->font = font;
	gwmPostUpdate(field);
};

void gwmClearTextFieldStyles(GWMTextField *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	while (data->styles != NULL)
	{
		TextStyle *style = data->styles;
		data->styles = style->next;
		free(style);
	};
	
	gwmPostUpdate(field);
};

static void removeStyle(GWMTextFieldData *data, size_t start, size_t end, int type)
{
	TextStyle *scan = data->styles;
	while (scan != NULL)
	{
		if (scan->type == type && scan->offset >= start && scan->offset < end)
		{
			if (data->styles == scan) data->styles = scan->next;
			if (scan->prev != NULL) scan->prev->next = scan->next;
			if (scan->next != NULL) scan->next->prev = scan->prev;
			
			TextStyle *next = scan->next;
			free(scan);
			scan = next;
		}
		else
		{
			scan = scan->next;
		};
	};
};

static void insertStyle(GWMTextFieldData *data, size_t pos, TextStyle *style)
{
	style->offset = pos;
	
	if (data->styles == NULL)
	{
		style->prev = NULL;
		style->next = NULL;
		data->styles = style;
	}
	else
	{
		TextStyle *scan = data->styles;
		while (scan != NULL)
		{
			if (scan->offset < pos)
			{
				if (scan->next == NULL)
				{
					style->prev = scan;
					style->next = NULL;
					scan->next = style;
					return;
				}
				else if (scan->next->offset >= pos)
				{
					style->prev = scan;
					style->next = scan->next;
					scan->next->prev = style;
					scan->next = style;
					return;
				};
			}
			else
			{
				style->prev = scan->prev;
				if (scan->prev == NULL) data->styles = style;
				style->next = scan;
				scan->prev->next = style;
				scan->prev = style;
				return;
			};
			
			scan = scan->next;
		};
	};
};

void gwmSetTextFieldColorRange(GWMTextField *field, size_t start, size_t end, DDIColor *color)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	
	size_t totalChars = ddiCountUTF8(data->text);
	if (end > totalChars) end = totalChars;
	
	TextStyle *style = (TextStyle*) malloc(sizeof(TextStyle));
	memset(style, 0, sizeof(TextStyle));
	
	style->type = TXT_STYLE_SET_FG;
	memcpy(&style->color, color, sizeof(DDIColor));
	
	// figure out the current color at 'end'
	DDIColor endColor = {0, 0, 0, 0xFF};
	TextStyle *scan;
	int needsEnd = 1;
	for (scan=data->styles; scan!=NULL; scan=scan->next)
	{
		if (scan->offset == end) needsEnd = 0;
		if (scan->offset >= end) break;
		if (scan->type == TXT_STYLE_SET_FG)
		{
			memcpy(&endColor, &scan->color, sizeof(DDIColor));
		};
	};
	
	// remove all foreground settings inbetween
	removeStyle(data, start, end, TXT_STYLE_SET_FG);
	
	// place our new style at 'start'
	insertStyle(data, start, style);
	
	// if needed, restore the original color at the end
	if (needsEnd)
	{
		style = (TextStyle*) malloc(sizeof(TextStyle));
		memset(style, 0, sizeof(TextStyle));

		style->type = TXT_STYLE_SET_FG;
		memcpy(&style->color, &endColor, sizeof(DDIColor));
		
		insertStyle(data, end, style);
	};
	
	gwmPostUpdate(field);
};
