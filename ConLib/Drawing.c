// ConLib - Win32 Replacement Console Library
// Copyright (C) 2009  David Quintana Conejero <gigaherz@gmail.com>
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// ConLib.cpp : Defines the exported functions for the DLL application.
//
#define _CRT_SECURE_NO_WARNINGS

#include "targetver.h"

#include <windows.h>
#include <tchar.h>
#include <stdarg.h>
#include <stdio.h>
#include "ConLibInternal.h"
#include <windowsx.h>

//#include <string>
//#include <set>

#include "UnicodeTools.h"

extern MSG __declspec(thread) messageToPost;

int clIsFullWidthStart(ConLibHandle handle, int x, int y)
{
	charAttribute attr;

	attr.all = handle->attributeRows[y][x];

	return attr.isFullWidthStart;
}

static void clRepaintText(ConLibHandle handle, HDC hdc, int x, int y, wchar_t* text, int nchars)
{
	TextOut(hdc,x * handle->characterWidth, y*handle->characterHeight, text, nchars);
}

static void clScrollBufferDown(ConLibHandle handle)
{
	int y, x;
	int* firstCRow = handle->characterRows[0];
	int* firstARow = handle->attributeRows[0];

	for(y=1;y<handle->bufferHeight;y++)
	{
		handle->characterRows[y-1] = handle->characterRows[y];
		handle->attributeRows[y-1] = handle->attributeRows[y];
	}
	handle->characterRows[handle->bufferHeight-1] = firstCRow;
	handle->attributeRows[handle->bufferHeight-1] = firstARow;
	for(x=0;x<handle->bufferWidth;x++)
	{
		firstCRow[x]=0x20; // space
		firstARow[x]=handle->defaultAttribute.all;
	}
}

static void clMoveDown(ConLibHandle handle)
{
	int YScrolled;

	handle->cursorY++;
	if(handle->cursorY>=handle->bufferHeight)
	{
		handle->cursorY--;
		clScrollBufferDown(handle);

		handle->scrollOffsetY = handle->bufferHeight-handle->windowHeight;
		SetScrollPos(handle->windowHandle,SB_VERT,handle->scrollOffsetY,TRUE);
		
		ScrollWindowEx(handle->windowHandle,0,-handle->characterHeight,NULL,NULL,NULL,NULL,SW_INVALIDATE);
	}

	YScrolled = handle->cursorY - handle->scrollOffsetY;

	if(YScrolled < 0)
	{
		int scrollDifference = YScrolled;

		if((-scrollDifference)>=handle->windowHeight)
			InvalidateRect(handle->windowHandle,NULL,FALSE);
		else
			ScrollWindowEx(handle->windowHandle,0,handle->characterHeight*scrollDifference,NULL,NULL,NULL,NULL,SW_INVALIDATE);

		handle->scrollOffsetY = handle->cursorY;
		//SetScrollPos(handle->windowHandle,SB_VERT,handle->scrollOffsetY,TRUE);
		clUpdateScrollBars(handle);
	}

	if(YScrolled >= handle->windowHeight)
	{
		int scrollDifference = YScrolled - handle->windowHeight + 1;

		if(scrollDifference>=handle->windowHeight)
			InvalidateRect(handle->windowHandle,NULL,FALSE);
		else
			ScrollWindowEx(handle->windowHandle,0,-handle->characterHeight*scrollDifference,NULL,NULL,NULL,NULL,SW_INVALIDATE);

		handle->scrollOffsetY = handle->cursorY - handle->windowHeight + 1;
		//SetScrollPos(handle->windowHandle,SB_VERT,handle->scrollOffsetY,TRUE);
		clUpdateScrollBars(handle);
	}

	if(handle->scrollOffsetY<0)
		handle->scrollOffsetY=0;
}

static void clMoveNext(ConLibHandle handle)
{
	handle->cursorX++;
	if(handle->cursorX>=handle->bufferWidth)
	{
		handle->cursorX=0;
		clMoveDown(handle);
	}
}

static void clPutChar(ConLibHandle handle, wchar_t chr)
{
	int* cRow = handle->characterRows[handle->cursorY];
	int* aRow = handle->attributeRows[handle->cursorY];

	if(chr <32)
	{
		switch(chr)
		{
		case 10:
			clMoveDown(handle);
			break;
		case 13:
			handle->cursorX=0;
			break;
		case 9:
			clMoveNext(handle);
			while( (handle->cursorX% TAB_SIZE)!= 0)
				clMoveNext(handle);
		}
	}
	else
	{
		RECT r = {
			handle->cursorX * handle->characterWidth,
			(handle->cursorY-handle->scrollOffsetY)*handle->characterHeight,
			(handle->cursorX+1) * handle->characterWidth,
			(handle->cursorY+1-handle->scrollOffsetY)*handle->characterHeight
		};

		cRow[handle->cursorX] = chr;
		aRow[handle->cursorX] = handle->currentAttribute.all;

		InvalidateRect(handle->windowHandle,&r,FALSE);

		clMoveNext(handle);
	}
}

int clInternalWrite(ConLibHandle handle, const wchar_t* data, int characters)
{
	int chars;

	for(chars=characters;chars>0;chars--)
	{
		wchar_t ch = *data++;
		int fw = IsFullWidth(ch);

		handle->currentAttribute.isFullWidthStart = fw;

		clPutChar(handle, ch);

		handle->currentAttribute.isFullWidthStart = 0;
			
		if(fw)
		{
			clPutChar(handle, FULLWIDTH_NOSPACE_FILLER);
		}
	}

//	if(characters>0)
//		InvalidateRect(handle->windowHandle,NULL, FALSE);

	return characters;
}

void clHandleWindowResize(ConLibHandle handle, int* width, int* height)
{
	int w = *width;
	int h = *height;

	int sw = w / handle->characterWidth;
	int sh = h / handle->characterHeight;

	if(sw < CONSOLE_MIN_WINDOW_WIDTH)	sw = CONSOLE_MIN_WINDOW_WIDTH;
	if(sh < CONSOLE_MIN_WINDOW_HEIGHT)	sh = CONSOLE_MIN_WINDOW_HEIGHT;
	if(sw > CONSOLE_MAX_WINDOW_WIDTH)	sw = CONSOLE_MAX_WINDOW_WIDTH;
	if(sh > CONSOLE_MAX_WINDOW_HEIGHT)	sh = CONSOLE_MAX_WINDOW_HEIGHT;

	if(sw > handle->bufferWidth)	sw = handle->bufferWidth;
	if(sh > handle->bufferHeight)	sh = handle->bufferHeight;

	if(*width == -1)		sw = handle->windowWidth;
	if(*height == -1)	sh = handle->windowHeight;

	*width = sw * handle->characterWidth;
	*height = sh * handle->characterHeight;

	handle->windowWidth = sw;
	handle->windowHeight = sh;

	if(handle->notificationCallback) 
		handle->notificationCallback(CONSOLE_NOTIFY_SIZE_CHANGED, sw, sh);
}

static void clRepaintArea(ConLibHandle handle, int sx, int sy, int ex, int ey)
{
	RECT r;
	
	r.left = (sx - handle->scrollOffsetX) * handle->characterWidth;
	r.top = (sy - handle->scrollOffsetY) * handle->characterHeight;
	r.right = (ex + 2 - handle->scrollOffsetX) * handle->characterWidth;
	r.bottom = (ey + 2 - handle->scrollOffsetY) * handle->characterHeight;

	InvalidateRect(handle->windowHandle,&r,TRUE);
}

// PRE: x1,x2,y1,y2 need to be in range of the buffer
static void clClearRect(ConLibHandle handle, int x1, int x2, int y1, int y2)
{
	int y,x;
	int attr = handle->defaultAttribute.all;
	for(y=y1;y<y2;y++)
	{	
		int* crow = handle->characterRows[y];
		int* arow = handle->attributeRows[y];
		for(x=x1;x<x2;x++)
		{
			crow[x]=0x20; // space
			arow[x]=attr;
		}
	}

	clRepaintArea(handle, x1, y1, x2, y2);
}

void clClearArea(ConLibHandle handle, int mode) 
{
	int firstLine=0;
	int lastLine=0;
	switch(mode)
	{
	case 0: // buffer
		firstLine = 0;
		lastLine = handle->bufferHeight;
		break;
	case 1: // visible
		firstLine = handle->scrollOffsetY;
		lastLine = firstLine + handle->windowHeight;
		break;
	case 2: // current line
		firstLine = handle->cursorY;
		lastLine = firstLine+1;
		break;
	}

	clClearRect(handle, 0, handle->bufferWidth, firstLine, lastLine);
}

void clPaintText(ConLibHandle handle, HWND hwnd)
{
	COLORREF bColor, fColor;
	int l,r,t,b;
	int selStartI, selEndI, selMode, bufWidth;
	int selStartX, selStartY, selEndX, selEndY;
	bool lastSelected;
	wchar_t* temp;
	int y, x;
	charAttribute lat;

	HDC hdc;
	PAINTSTRUCT paint;
	
	hdc = BeginPaint(hwnd,&paint);
	{
		if(hdc==NULL)
			return;

		l = paint.rcPaint.left/handle->characterWidth;
		r = (paint.rcPaint.right+handle->characterWidth-1)/handle->characterWidth;

		t = paint.rcPaint.top/handle->characterHeight;
		b = (paint.rcPaint.bottom+handle->characterHeight-1)/handle->characterHeight;

		if(r > handle->windowWidth)
			r=handle->windowWidth;
		if(b > handle->windowHeight)
			b=handle->windowHeight;

		lat.all = 0;

		bColor = RGB((lat.bgColorR*255)/31,(lat.bgColorG*255)/31,(lat.bgColorB*255)/31);
		fColor = RGB((lat.fgColorR*255)/31,(lat.fgColorG*255)/31,(lat.fgColorB*255)/31);

		// for sel modes 1 and 2
		selStartI = 0;
		selEndI   = -1;
		selMode   = handle->selectionMode;
		bufWidth  = handle->bufferWidth;

		selStartX = handle->selectionStartX;
		selStartY = handle->selectionStartY;
		selEndX = handle->selectionEndX;
		selEndY = handle->selectionEndY;

		if(selEndX < selStartX) swap(&selStartX, &selEndX);
		if(selEndY < selStartY) swap(&selStartY, &selEndY);

		switch(selMode)
		{
		case 1:
			selStartI = (selStartY * bufWidth) + selStartX;
			selEndI   = (selEndY * bufWidth) + selEndX;
			break;
		case 2:
			selStartI = (selStartY * bufWidth);
			selEndI   = ((selEndY + 1) * bufWidth) - 1;
			break;
		}

		lastSelected = false;

		SelectObject(hdc,(lat.bold?handle->fontBold:handle->fontNormal));

		SetTextColor(hdc, fColor);
		SetBkColor(hdc, bColor);

		temp = (wchar_t*)malloc((handle->windowWidth+1)*sizeof(wchar_t));
		for(y=t;y<b;y++)
		{
			int ay = y+handle->scrollOffsetY;

			int* cRow = handle->characterRows[ay];
			int* aRow = handle->attributeRows[ay];
			int nchars=0;
			int lastx=l;
			
			if(l > 0 || handle->scrollOffsetX > 0)
			{
				if(clIsFullWidthStart(handle, l+handle->scrollOffsetX-1, ay))
				{
					lastx--;
				}
			}

			for(x=lastx;x<r;x++)
			{
				bool isSelected = false;
				charAttribute at;
				
				int ax = x+handle->scrollOffsetX;

				at.all = aRow[ax];

				if(selMode==1)
				{
					int cI = (ay * bufWidth) + ax;
					isSelected = (cI>=selStartI) && (cI<=selEndI);
				}
				else if(selMode==2)
				{
					isSelected = (ay >= selStartY) && (ay <= selEndY);
				}
				else if(selMode==3)
				{
					isSelected = (ay >= selStartY) && (ay <= selEndY) && (ax >= selStartX) && (ax <= selEndX);
				}

				if((at.fg != lat.fg) || (at.fg != lat.fg) || (at.bold != lat.bold) || (lastSelected != isSelected))
				{
					temp[nchars]=0;
					if(nchars>0)
					{
						clRepaintText(handle, hdc, lastx, y, temp, nchars);
					}
					nchars=0;
					lastx=x;

					if(isSelected)
					{
						int t = lat.bgColorB + lat.bgColorG + lat.bgColorR;

						if(t > 48)
						{
							fColor = 0x00ffffff;
							bColor = 0x00000000;
						}
						else
						{
							bColor = 0x00ffffff;
							fColor = 0x00000000;
						}
					}
					else
					{
						bColor = RGB((at.bgColorR*255)/31,(at.bgColorG*255)/31,(at.bgColorB*255)/31);
						fColor = RGB((at.fgColorR*255)/31,(at.fgColorG*255)/31,(at.fgColorB*255)/31);
					}

					SelectObject(hdc,(at.bold?handle->fontBold:handle->fontNormal));
					SetTextColor(hdc, fColor);
					SetBkColor(hdc, bColor);
				}

				if(!lat.isFullWidthStart)
					temp[nchars++] = (wchar_t)cRow[ax];
				
				lat=at;
				lastSelected = isSelected;
			}
			temp[nchars]=0;
			if(nchars>0)
			{
				clRepaintText(handle, hdc, lastx, y, temp, nchars);
			}
		}
		free(temp);
	}	
	EndPaint(hwnd,&paint);
}
