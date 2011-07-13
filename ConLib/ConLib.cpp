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
#include "targetver.h"

#include <windows.h>
#include <tchar.h>
#include <stdarg.h>
#include <stdio.h>
#include "ConLibInternal.h"
#include <windowsx.h>

#include <string>
#include <set>

#define TAB_SIZE 8
#define FULLWIDTH_NOSPACE_FILLER 0x2060

bool classRegistered=false;

MSG __declspec(thread) messageToPost;

LONG timerNextID=0;

static bool IsFullWidth(wchar_t character)
{
   /* Taken from:
	* EastAsianWidth-6.0.0.txt
	* Date: 2010-08-17, 12:17:00 PDT [KW]
	*
	* East Asian Width Properties
	*
	* This file is an informative contributory data file in the
	* Unicode Character Database.
	*
	* Copyright (c) 1991-2010 Unicode, Inc.
	* For terms of use, see http://www.unicode.org/terms_of_use.html
	*/
#define RANGE(a,b) ((character >= a) && (character <= b))
#define SINGLE(a) (character == a)
#if 0
	// these ranges are "definately no"
	if(RANGE(0x0020,0x007E)) return false;
	if(RANGE(0x00A2,0x00A3)) return false;
	if(RANGE(0x00A5,0x00A6)) return false;
	if(SINGLE(0x00AC)) return false;
	if(SINGLE(0x00AF)) return false;
	if(SINGLE(0x20A9)) return false;
	if(RANGE(0x27E6,0x27ED)) return false;
	if(RANGE(0x2985,0x2986)) return false;
	if(RANGE(0xFF61,0xFFBE)) return false;
	if(RANGE(0xFFC2,0xFFC7)) return false;
	if(RANGE(0xFFCA,0xFFCF)) return false;
	if(RANGE(0xFFD2,0xFFD7)) return false;
	if(RANGE(0xFFDA,0xFFDC)) return false;
	if(RANGE(0xFFE8,0xFFEE)) return false;
#endif
	if(character < 0x8000)
	{
		if(character < 0x3000)
		{
			if(character < 0x2800)
			{
				if(RANGE(0x1100,0x115F)) return true;
				if(RANGE(0x11A3,0x11A7)) return true;
				if(RANGE(0x11FA,0x11FF)) return true;
				if(RANGE(0x2329,0x232A)) return true;
			}
			else
			{
				if(RANGE(0x2E80,0x2E99)) return true;
				if(RANGE(0x2E9B,0x2EF3)) return true;
				if(RANGE(0x2F00,0x2FD5)) return true;
				if(RANGE(0x2FF0,0x2FFB)) return true;
			}
		}
		else
		{
			if(character < 0x3200)
			{
				if(character < 0x3130)
				{
					if(RANGE(0x3000,0x303E)) return true;
					if(RANGE(0x3041,0x3096)) return true;
					if(RANGE(0x3099,0x30FF)) return true;
					if(RANGE(0x3105,0x312D)) return true;
				}
				else
				{
					if(RANGE(0x3131,0x318E)) return true;
					if(RANGE(0x3190,0x31BA)) return true;
					if(RANGE(0x31C0,0x31E3)) return true;
					if(RANGE(0x31F0,0x31FF)) return true;
				}
			}
			else
			{
				if(RANGE(0x3200,0x321E)) return true;
				if(RANGE(0x3220,0x3247)) return true;
				if(RANGE(0x3250,0x32FE)) return true;
				if(RANGE(0x3300,0x33FF)) return true;
				if(RANGE(0x3400,0x4DB5)) return true;
				if(RANGE(0x4DB6,0x4DBF)) return true;
				if(RANGE(0x4E00,0x7FFF)) return true;
			}
		}
	}
	else
	{
		if(character < 0xF000)
		{
			if(character < 0xA800)
			{
				if(RANGE(0x8000,0x9FCB)) return true;
				if(RANGE(0x9FCC,0x9FFF)) return true;
				if(RANGE(0xA000,0xA48C)) return true;
				if(RANGE(0xA490,0xA4C6)) return true;
			}
			else
			{
				if(RANGE(0xA960,0xA97C)) return true;
				if(RANGE(0xAC00,0xD7A3)) return true;
				if(RANGE(0xD7B0,0xD7C6)) return true;
				if(RANGE(0xD7CB,0xD7FB)) return true;
			}
		}
		else
		{
			if(character < 0xFE00)
			{
				if(RANGE(0xF900,0xFA2D)) return true;
				if(RANGE(0xFA2E,0xFA2F)) return true;
				if(RANGE(0xFA30,0xFA6D)) return true;
				if(RANGE(0xFA6E,0xFA6F)) return true;
				if(RANGE(0xFA70,0xFAD9)) return true;
				if(RANGE(0xFADA,0xFAFF)) return true;
			}
			else
			{
				if(RANGE(0xFE10,0xFE19)) return true;
				if(RANGE(0xFE30,0xFE52)) return true;
				if(RANGE(0xFE54,0xFE66)) return true;
				if(RANGE(0xFE68,0xFE6B)) return true;
				if(RANGE(0xFF01,0xFF60)) return true;
				if(RANGE(0xFFE0,0xFFE6)) return true;
			}
		}
	}
#if 0
	// these ranges are on pages > the win32 wchar_t range,
	// no point enabling them since chars > 0xFFFF won't be recognized by my code.
	if(RANGE(0x1B000,0x1B001)) return true;
	if(RANGE(0x1F200,0x1F202)) return true;
	if(RANGE(0x1F210,0x1F23A)) return true;
	if(RANGE(0x1F240,0x1F248)) return true;
	if(RANGE(0x1F250,0x1F251)) return true;
	if(RANGE(0x20000,0x2A6D6)) return true;
	if(RANGE(0x2A6D7,0x2A6FF)) return true;
	if(RANGE(0x2A700,0x2B734)) return true;
	if(RANGE(0x2B735,0x2F73F)) return true;
	if(RANGE(0x2B740,0x2B81D)) return true;
	if(RANGE(0x2B81E,0x2F7FF)) return true;
	if(RANGE(0x2F800,0x2FA1D)) return true;
	if(RANGE(0x2FA1E,0x2FFFD)) return true;
	if(RANGE(0x30000,0x3FFFD)) return true;
#endif
	// default to no
	return false;
}

static void ConLibRepaintText(ConLibHandle handle, HDC hdc, int x, int y, wchar_t* text, int nchars)
{
	TextOut(hdc,x * handle->characterWidth, y*handle->characterHeight, text, nchars);
}

static void ConLibUpdateScrollBars(ConLibHandle handle)
{
	HWND hwnd = handle->windowHandle;

	SCROLLINFO si;
	ZeroMemory(&si, sizeof(si));

	if(handle->windowHeight>=handle->bufferHeight)
	{
		handle->scrollOffsetY=0;
		//ShowScrollBar(hwnd,SB_VERT,FALSE);
		si.cbSize = sizeof(si);
		si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS | SIF_TRACKPOS | SIF_DISABLENOSCROLL;
		si.nMin = 0;
		si.nMax = 0;
		si.nPage = 1;
		si.nPos = 0;
		si.nTrackPos=0;
		SetScrollInfo(hwnd,SB_VERT, &si, TRUE);

	}
	else
	{
		if(handle->scrollOffsetY>=(handle->bufferHeight-handle->windowHeight))
			handle->scrollOffsetY = handle->bufferHeight-handle->windowHeight-1;

		//ShowScrollBar(hwnd,SB_VERT,TRUE);

		si.cbSize = sizeof(si);
		si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS | SIF_TRACKPOS;
		si.nMin = 0;
		si.nMax = handle->bufferHeight-1;
		si.nPage = handle->windowHeight;
		si.nPos = handle->scrollOffsetY;
		si.nTrackPos = handle->scrollOffsetY;
		SetScrollInfo(hwnd,SB_VERT, &si, TRUE);
		EnableScrollBar(hwnd, SB_VERT, ESB_ENABLE_BOTH);
	}

	if(handle->windowWidth>=handle->bufferWidth)
	{
		handle->scrollOffsetX=0;
		ShowScrollBar(hwnd,SB_HORZ,FALSE);
	}
	else
	{
		if(handle->scrollOffsetX>=(handle->bufferWidth-handle->windowWidth))
			handle->scrollOffsetX = handle->bufferWidth-handle->windowWidth-1;

		ShowScrollBar(hwnd,SB_HORZ,TRUE);

		si.cbSize = sizeof(si);
		si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS | SIF_TRACKPOS;
		si.nMin = 0;
		si.nMax = handle->bufferWidth-1;
		si.nPage = handle->windowWidth;
		si.nPos = handle->scrollOffsetX;
		si.nTrackPos = handle->scrollOffsetX;
		SetScrollInfo(hwnd,SB_HORZ, &si, TRUE);
	}
}

static void ConLibScrollBufferDown(ConLibHandle handle)
{
	int* firstCRow = handle->characterRows[0];
	int* firstARow = handle->attributeRows[0];

	for(int y=1;y<handle->bufferHeight;y++)
	{
		handle->characterRows[y-1] = handle->characterRows[y];
		handle->attributeRows[y-1] = handle->attributeRows[y];
	}
	handle->characterRows[handle->bufferHeight-1] = firstCRow;
	handle->attributeRows[handle->bufferHeight-1] = firstARow;
	for(int x=0;x<handle->bufferWidth;x++)
	{
		firstCRow[x]=0x20; // space
		firstARow[x]=handle->defaultAttribute.all;
	}
}

static void ConLibMoveDown(ConLibHandle handle)
{
	handle->cursorY++;
	if(handle->cursorY>=handle->bufferHeight)
	{
		handle->cursorY--;
		ConLibScrollBufferDown(handle);

		handle->scrollOffsetY = handle->bufferHeight-handle->windowHeight;
		SetScrollPos(handle->windowHandle,SB_VERT,handle->scrollOffsetY,TRUE);
		
		ScrollWindowEx(handle->windowHandle,0,-handle->characterHeight,NULL,NULL,NULL,NULL,SW_INVALIDATE);
	}

	int YScrolled = handle->cursorY - handle->scrollOffsetY;

	if(YScrolled < 0)
	{
		int scrollDifference = YScrolled;

		if((-scrollDifference)>=handle->windowHeight)
			InvalidateRect(handle->windowHandle,NULL,FALSE);
		else
			ScrollWindowEx(handle->windowHandle,0,handle->characterHeight*scrollDifference,NULL,NULL,NULL,NULL,SW_INVALIDATE);

		handle->scrollOffsetY = handle->cursorY;
		//SetScrollPos(handle->windowHandle,SB_VERT,handle->scrollOffsetY,TRUE);
		ConLibUpdateScrollBars(handle);
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
		ConLibUpdateScrollBars(handle);
	}

	if(handle->scrollOffsetY<0)
		handle->scrollOffsetY=0;
}

static void ConLibMoveNext(ConLibHandle handle)
{
	handle->cursorX++;
	if(handle->cursorX>=handle->bufferWidth)
	{
		handle->cursorX=0;
		ConLibMoveDown(handle);
	}
}

static void ConLibPutChar(ConLibHandle handle, wchar_t chr)
{
	int* cRow = handle->characterRows[handle->cursorY];
	int* aRow = handle->attributeRows[handle->cursorY];

	if(chr <32)
	{
		switch(chr)
		{
		case 10:
			ConLibMoveDown(handle);
			break;
		case 13:
			handle->cursorX=0;
			break;
		case 9:
			ConLibMoveNext(handle);
			while( (handle->cursorX% TAB_SIZE)!= 0)
				ConLibMoveNext(handle);
		}
	}
	else
	{
		cRow[handle->cursorX] = chr;
		aRow[handle->cursorX] = handle->currentAttribute.all;

		RECT r = {
			handle->cursorX * handle->characterWidth,
			(handle->cursorY-handle->scrollOffsetY)*handle->characterHeight,
			(handle->cursorX+1) * handle->characterWidth,
			(handle->cursorY+1-handle->scrollOffsetY)*handle->characterHeight
		};

		InvalidateRect(handle->windowHandle,&r,FALSE);

		ConLibMoveNext(handle);
	}
}

static int ConLibInternalWrite(ConLibHandle handle, const wchar_t* data, int characters)
{
	int chars=characters;
	for(int chars=characters;chars>0;chars--)
	{
		wchar_t ch = *data++;
		ConLibPutChar(handle, ch);
		if(IsFullWidth(ch))
			ConLibPutChar(handle, FULLWIDTH_NOSPACE_FILLER);
	}
//	if(characters>0)
//		InvalidateRect(handle->windowHandle,NULL, FALSE);

	return characters;
}

static int ConLibCopyDataHandler(ConLibHandle handle, COPYDATASTRUCT* cds)
{
	if(cds->dwData == CONSOLE_DATA_ANSI)
	{
		wchar_t *data = (wchar_t*)alloca(cds->cbData*4);
		int nch = MultiByteToWideChar(CP_THREAD_ACP, MB_PRECOMPOSED, (LPCSTR)cds->lpData, cds->cbData, data, cds->cbData*2);
		data[nch]=0;

		return ConLibInternalWrite(handle,data,nch);
	}
	else if(cds->dwData == CONSOLE_DATA_UNICODE)
	{
		return ConLibInternalWrite(handle,(wchar_t*)cds->lpData,cds->cbData>>1);
	}
	return -1;
}

static void ConLibHandleWindowResize(ConLibHandle handle, int& width, int& height)
{
	int w = width;
	int h = height;

	int sw = w / handle->characterWidth;
	int sh = h / handle->characterHeight;

	if(sw < CONSOLE_MIN_WINDOW_WIDTH)	sw = CONSOLE_MIN_WINDOW_WIDTH;
	if(sh < CONSOLE_MIN_WINDOW_HEIGHT)	sh = CONSOLE_MIN_WINDOW_HEIGHT;
	if(sw > CONSOLE_MAX_WINDOW_WIDTH)	sw = CONSOLE_MAX_WINDOW_WIDTH;
	if(sh > CONSOLE_MAX_WINDOW_HEIGHT)	sh = CONSOLE_MAX_WINDOW_HEIGHT;

	if(sw > handle->bufferWidth)	sw = handle->bufferWidth;
	if(sh > handle->bufferHeight)	sh = handle->bufferHeight;

	if(width == -1)		sw = handle->windowWidth;
	if(height == -1)	sh = handle->windowHeight;

	width = sw * handle->characterWidth;
	height = sh * handle->characterHeight;

	handle->windowWidth = sw;
	handle->windowHeight = sh;

}

static void ConLibProcessResize(ConLibHandle handle, RECT* r, int wParam)
{
	//ConLibUpdateScrollBars(handle);

	RECT wr,cr;
	if(GetWindowRect(handle->windowHandle,&wr)==0) return;
	if(GetClientRect(handle->windowHandle,&cr)==0) return;

	int xborder = (wr.right - wr.left) - (cr.right - cr.left);
	int yborder = (wr.bottom - wr.top) - (cr.bottom - cr.top);

	int yhscroll = GetSystemMetrics(SM_CYHSCROLL);

	int cwidth = (r->right - r->left) - xborder;
	int cheight = (r->bottom - r->top) - yborder;

	int width = cwidth + (handle->characterWidth / 2);
	int height = cheight + (handle->characterHeight / 2);

	bool hadHS = (handle->windowWidth == handle->bufferWidth);
	//bool hadVS = (handle->windowHeight == handle->windowHeight);

	ConLibHandleWindowResize(handle, width, height);
	ConLibUpdateScrollBars(handle);

	bool newHS = (handle->windowWidth == handle->bufferWidth);
	//bool newVS = (handle->windowHeight == handle->windowHeight);

	if(hadHS && !newHS) // we don't need the HS anymore
	{
		height += yhscroll;
		yborder -= yhscroll; // compensate border differences

		ConLibHandleWindowResize(handle, width, height);
	}

	if(!hadHS && newHS) // added a scrollbar
	{
		height -= handle->characterHeight * (yhscroll / handle->characterHeight); 
		yborder += yhscroll; // compensate border differences

		ConLibHandleWindowResize(handle, width, height);
	}

	width += xborder;
	height += yborder;

	if( (wParam == WMSZ_LEFT) || (wParam == WMSZ_TOPLEFT) || (wParam == WMSZ_BOTTOMLEFT)  ) // left
	{
		r->left = r->right - width;
	}
	else
	{
		r->right = r->left + width;
	}

	if( (wParam == WMSZ_TOP) || (wParam == WMSZ_TOPLEFT) || (wParam == WMSZ_TOPRIGHT)  ) // top
	{
		r->top = r->bottom - height;
	}	
	else
	{
		r->bottom = r->top + height;
	}

}

static void ConLibRepaintSelection(ConLibHandle handle, int sx, int sy, int ex, int ey)
{
	RECT r;
	
	r.left = (sx - handle->scrollOffsetX) * handle->characterWidth;
	r.top = (sy - handle->scrollOffsetY) * handle->characterHeight;
	r.right = (ex + 2 - handle->scrollOffsetX) * handle->characterWidth;
	r.bottom = (ey + 2 - handle->scrollOffsetY) * handle->characterHeight;

	InvalidateRect(handle->windowHandle,&r,TRUE);
}

void TimedPostMessage(ConLibHandle handle, int delay, HWND hwnd, int msg, WPARAM wParam, LPARAM lParam)
{
	messageToPost.hwnd = hwnd;
	messageToPost.message = msg;
	messageToPost.wParam = wParam;
	messageToPost.lParam = lParam;

	LONG id = InterlockedExchangeAdd(&timerNextID,1);

	SetTimer(hwnd,handle->idThread,500,NULL);
};


static LRESULT CALLBACK ConLibWndProc(HWND hwnd, UINT message,
								WPARAM wParam, LPARAM lParam)
{
	ConLibHandle handle = (ConLibHandle)GetWindowLongPtr(hwnd, GWL_USERDATA);

	switch(message)
	{
	case WM_CREATE:
		{
			LPCREATESTRUCT lpcs = (LPCREATESTRUCT)lParam;
			SetWindowLongPtr(hwnd, GWL_USERDATA, (LONG_PTR)lpcs->lpCreateParams);
		}
		break;

	case WM_CLOSE:
		if(handle->notificationCallback) 
			return handle->notificationCallback(CONSOLE_NOTIFY_CLOSE, wParam, lParam);
		break;
	case WM_COPYDATA:
		return ConLibCopyDataHandler(handle, (COPYDATASTRUCT*)lParam);

	case WM_LBUTTONDOWN:

		SetCapture(hwnd);

		handle->selectionMode = 1;
		if(wParam&MK_CONTROL)
			handle->selectionMode = 2;
		else if(wParam&MK_SHIFT)
			handle->selectionMode = 3;

		handle->selectionStartX = (GET_X_LPARAM(lParam) / handle->characterWidth)+handle->scrollOffsetX; 
		handle->selectionStartY = (GET_Y_LPARAM(lParam) / handle->characterHeight)+handle->scrollOffsetY; 
		handle->selectionEndX = handle->selectionStartX; 
		handle->selectionEndY = handle->selectionStartY; 
		handle->mouseLIsPressed = true;

		InvalidateRect(hwnd,NULL,FALSE);

		break;

	case WM_TIMER:
		return ConLibWndProc(messageToPost.hwnd,messageToPost.message,messageToPost.wParam,messageToPost.lParam);

	case WM_MOUSEMOVE:
		KillTimer(hwnd,handle->idThread);
		if((handle->selectionMode>0)&&(handle->mouseLIsPressed))
		{
			handle->selectionEndX = (GET_X_LPARAM(lParam) / handle->characterWidth)+handle->scrollOffsetX; 
			handle->selectionEndY = (GET_Y_LPARAM(lParam) / handle->characterHeight)+handle->scrollOffsetY; 

			if(handle->selectionEndX < handle->scrollOffsetX)
			{
				handle->scrollOffsetX = handle->selectionEndX;

				if(handle->scrollOffsetX < 0)
					handle->scrollOffsetX = 0;
				else
					TimedPostMessage(handle, 200, hwnd, WM_MOUSEMOVE, wParam,lParam);
			}

			if(handle->selectionEndX >= (handle->scrollOffsetX+handle->windowWidth))
			{
				handle->scrollOffsetX = handle->selectionEndX - handle->windowWidth + 1;

				if((handle->scrollOffsetX + handle->windowWidth) > handle->bufferWidth)
					handle->scrollOffsetX = handle->bufferWidth - handle->windowWidth + 1;
				else
					TimedPostMessage(handle, 200, hwnd, WM_MOUSEMOVE, wParam,lParam);
			}

			if(handle->selectionEndY < handle->scrollOffsetY)
			{
				handle->scrollOffsetY = handle->selectionEndY;

				if(handle->scrollOffsetY < 0)
					handle->scrollOffsetY = 0;
				else
					TimedPostMessage(handle, 200, hwnd, WM_MOUSEMOVE, wParam,lParam);
			}

			if(handle->selectionEndY >= (handle->scrollOffsetY+handle->windowHeight))
			{
				handle->scrollOffsetY = handle->selectionEndY - handle->windowHeight + 1;

				if((handle->scrollOffsetY + handle->windowHeight) > handle->bufferHeight)
					handle->scrollOffsetY = handle->bufferHeight - handle->windowHeight + 1;
				else
					TimedPostMessage(handle, 200, hwnd, WM_MOUSEMOVE, wParam,lParam);
			}

			InvalidateRect(hwnd,NULL,FALSE);
		}

		break;

	case WM_LBUTTONUP:

		if(handle->selectionMode>0)
		{
			handle->selectionEndX = (GET_X_LPARAM(lParam) / handle->characterWidth)+handle->scrollOffsetX; 
			handle->selectionEndY = (GET_Y_LPARAM(lParam) / handle->characterHeight)+handle->scrollOffsetY; 
			InvalidateRect(hwnd,NULL,FALSE);
		}
		handle->mouseLIsPressed = false;

		KillTimer(hwnd,handle->idThread);
		ReleaseCapture();
		break;

	case WM_KEYDOWN:

		if((handle->mouseLIsPressed)&&(handle->selectionMode > 0))
		{
			if(wParam == VK_CONTROL)
			{
				handle->ctrlIsPressed = true;
				handle->selectionMode = 2;
			}
			else if(wParam == VK_SHIFT)
			{
				handle->shiftIsPressed = true;
				handle->selectionMode = 3;
			}
			InvalidateRect(hwnd,NULL,FALSE);
		}
		else
		{
			if(wParam == VK_CONTROL)
				handle->ctrlIsPressed = true;
			else if(wParam == VK_SHIFT)
				handle->shiftIsPressed = true;
		}
		break;

	case WM_KEYUP:

		if((handle->mouseLIsPressed)&&(handle->selectionMode > 0))
		{
			if(wParam == VK_CONTROL)
			{
				handle->ctrlIsPressed = false;
				handle->selectionMode = 1;

				if(handle->shiftIsPressed)
					handle->selectionMode = 3;
			}
			else if(wParam == VK_SHIFT)
			{
				handle->shiftIsPressed = false;
				handle->selectionMode = 1;

				if(handle->ctrlIsPressed)
					handle->selectionMode = 2;
			}
			InvalidateRect(hwnd,NULL,FALSE);
		}
		else
		{
			if(wParam == VK_CONTROL)
				handle->ctrlIsPressed = false;
			else if(wParam == VK_SHIFT)
				handle->shiftIsPressed = false;
		}

		if(wParam == VK_ESCAPE)
		{
			if(handle->selectionMode>0)
			{
				handle->selectionMode = false;
				InvalidateRect(hwnd,NULL,FALSE);
			}
		}

		if((wParam == 'C') && (handle->ctrlIsPressed) && (handle->selectionMode > 0))
		{
			std::wstring copiedText = L"";

			switch(handle->selectionMode)
			{
			case 1: // inline
				{
					for(int x=handle->selectionStartX;x<handle->bufferWidth;x++)
					{
						copiedText += (wchar_t)(handle->characterRows[handle->selectionStartY][x]);
					}
					if (handle->selectionStartY < handle->selectionEndY)
					{
						copiedText += L"\r\n";
						for(int y=handle->selectionStartY+1;y<handle->selectionEndY;y++)
						{
							for(int x=0;x<handle->bufferWidth;x++)
							{
								copiedText += (wchar_t)(handle->characterRows[y][x]);
							}
							copiedText += L"\r\n";
						}
						for(int x=0;x<=handle->selectionEndX;x++)
						{
							copiedText += (wchar_t)(handle->characterRows[handle->selectionEndY][x]);
						}
					}
				}
				break;
			case 2: // whole lines
				{
					for(int y=handle->selectionStartY;y<=handle->selectionEndY;y++)
					{
						for(int x=0;x<handle->bufferWidth;x++)
						{
							copiedText += (wchar_t)(handle->characterRows[y][x]);
						}
						copiedText += L"\r\n";
					}
				}
				break;
			case 3: // rectangle
				{
					for(int y=handle->selectionStartY;y<=handle->selectionEndY;y++)
					{
						for(int x=handle->selectionStartY;x<=handle->selectionEndY;x++)
						{
							copiedText += (wchar_t)(handle->characterRows[y][x]);
						}
						copiedText += L"\r\n";
					}
				}
				break;
			}

			if(copiedText.length() > 0)
			{
				if(OpenClipboard(hwnd))
				{
					EmptyClipboard();

					if(HANDLE hMem = GlobalAlloc(GMEM_MOVEABLE, (copiedText.length()+1) * sizeof(wchar_t)))
					{
						wchar_t* ptr = (wchar_t*)GlobalLock(hMem);
						CopyMemory(ptr,copiedText.c_str(),(copiedText.length()+1) * sizeof(wchar_t));
						ptr[copiedText.length()]=0;
						GlobalUnlock(hMem);

						SetClipboardData(CF_UNICODETEXT,hMem);
					}

					if(HANDLE hMem = GlobalAlloc(GMEM_MOVEABLE, copiedText.length()+1))
					{
						char* ptr = (char*)GlobalLock(hMem);
						WideCharToMultiByte(CP_THREAD_ACP,WC_COMPOSITECHECK,copiedText.c_str(),copiedText.length(),ptr,copiedText.length()+1,NULL,NULL);
						ptr[copiedText.length()]=0;
						GlobalUnlock(hMem);

						SetClipboardData(CF_TEXT,hMem);
					}

					CloseClipboard();
				}
			}

			handle->selectionMode = false;
			InvalidateRect(hwnd,NULL,FALSE);
		}

		break;
	case WM_GETMINMAXINFO:
		{
			MINMAXINFO *mmi=(MINMAXINFO*)lParam;

			if(!handle)
				break;

			// make sure scrollbar state is updated
			ConLibUpdateScrollBars(handle);

			RECT wr,cr;
			if(GetWindowRect(handle->windowHandle,&wr)==0) break;
			if(GetClientRect(handle->windowHandle,&cr)==0) break;

			int xborder = (wr.right - wr.left) - (cr.right - cr.left);
			int yborder = (wr.bottom - wr.top) - (cr.bottom - cr.top);

			int maxx = xborder + (handle->bufferWidth * handle->characterWidth);
			int maxy = yborder + (handle->bufferHeight * handle->characterHeight);

			int cxm = handle->characterWidth * (GetSystemMetrics(SM_CXMAXIMIZED)/handle->characterWidth);
			int cym = handle->characterHeight * (GetSystemMetrics(SM_CYMAXIMIZED)/handle->characterHeight);

			mmi->ptMaxSize.x = min(cxm,maxx);
			mmi->ptMaxSize.y = min(cym,maxy);
			mmi->ptMaxTrackSize = mmi->ptMaxSize;
		}
		break;

	case WM_SIZING:

		if(handle->lastWindowState != wParam)
			InvalidateRect(hwnd,NULL,TRUE);

		handle->lastWindowState = (int)wParam;

		if(wParam != SIZE_MINIMIZED)
		{
			RECT* r = (RECT*)lParam;

			ConLibProcessResize(handle, r, wParam);
		}

		break;

	case WM_SIZE:

		if( (wParam != SIZE_MINIMIZED) && (handle->lastWindowState != wParam))
		{
			RECT r;

			if(GetWindowRect(hwnd,&r)==0) break;

			ConLibProcessResize(handle, &r, 0);

			ConLibUpdateScrollBars(handle);

			InvalidateRect(hwnd,NULL,TRUE);
			UpdateWindow(hwnd);
		}

		handle->lastWindowState = (int)wParam;
		break;

	case WM_HSCROLL:
		{
			int sbValue;
			int cmd = LOWORD(wParam);
			SCROLLINFO si;
			ZeroMemory(&si,sizeof(si));
			si.cbSize = sizeof(si);
			si.fMask = SIF_POS|SIF_PAGE|SIF_RANGE|SIF_TRACKPOS;
				
			GetScrollInfo(hwnd, SB_HORZ, &si);
			
			sbValue = si.nPos;
				
			switch(cmd)
			{
			case SB_BOTTOM:
				sbValue = si.nMax;
				break;

			case SB_TOP:
				sbValue = si.nMax;
				break;

			case SB_LINEDOWN:       //Scrolls one line down.
				sbValue++;
				break;

			case SB_LINEUP:         //Scrolls one line up.
				sbValue--;
				break;

			case SB_PAGEDOWN:       //Scrolls one page down.
				sbValue += si.nPage;
				break;

			case SB_PAGEUP:         //Scrolls one page up.
				sbValue -= si.nPage;
				break;

			case SB_THUMBTRACK:
			case SB_THUMBPOSITION:
				sbValue = si.nTrackPos;
				break;
			}

			if(sbValue != handle->scrollOffsetX)
			{
				if(sbValue > (handle->bufferHeight - handle->windowHeight))
					sbValue = (handle->bufferHeight - handle->windowHeight);

				if(sbValue < 0)
					sbValue = 0;

				handle->scrollOffsetX = sbValue;
				SetScrollPos(hwnd,SB_HORZ,sbValue,TRUE);
				InvalidateRect(hwnd,NULL,FALSE);
			}
		}
		return 0;
	case WM_VSCROLL:
		{			
			int sbValue;
			int cmd = LOWORD(wParam);
			SCROLLINFO si;
			ZeroMemory(&si,sizeof(si));
			si.cbSize = sizeof(si);
			si.fMask = SIF_POS|SIF_PAGE|SIF_RANGE|SIF_TRACKPOS;

			GetScrollInfo(hwnd, SB_VERT, &si);
				
			sbValue = si.nPos;
				
			switch(cmd)
			{
			case SB_BOTTOM:
				sbValue = si.nMax;
				break;

			case SB_TOP:
				sbValue = si.nMax;
				break;

			case SB_LINEDOWN:       //Scrolls one line down.
				sbValue++;
				break;

			case SB_LINEUP:         //Scrolls one line up.
				sbValue--;
				break;

			case SB_PAGEDOWN:       //Scrolls one page down.
				sbValue += si.nPage;
				break;

			case SB_PAGEUP:         //Scrolls one page up.
				sbValue -= si.nPage;
				break;

			case SB_THUMBTRACK:
			case SB_THUMBPOSITION:
				sbValue = si.nTrackPos;
				break;
			}

			if(sbValue != handle->scrollOffsetY)
			{
				if(sbValue > (handle->bufferHeight - handle->windowHeight))
					sbValue = (handle->bufferHeight - handle->windowHeight);

				if(sbValue < 0)
					sbValue = 0;

				handle->scrollOffsetY = sbValue;
				SetScrollPos(hwnd,SB_VERT,sbValue,TRUE);
				InvalidateRect(hwnd,NULL,FALSE);
			}
		}
		return 0;
	case WM_PAINT:

		PAINTSTRUCT paint;

		ConLibUpdateScrollBars(handle);

		HDC hdc = BeginPaint(hwnd,&paint);

		if(hdc==NULL)
			break;

		int l = paint.rcPaint.left/handle->characterWidth;
		int r = (paint.rcPaint.right+handle->characterWidth-1)/handle->characterWidth;

		int t = paint.rcPaint.top/handle->characterHeight;
		int b = (paint.rcPaint.bottom+handle->characterHeight-1)/handle->characterHeight;

		if(r > handle->windowWidth)
			r=handle->windowWidth;
		if(b > handle->windowHeight)
			b=handle->windowHeight;

		charAttribute lat=handle->currentAttribute;

		COLORREF bColor = RGB((lat.bgColorR*255)/31,(lat.bgColorG*255)/31,(lat.bgColorB*255)/31);
		COLORREF fColor = RGB((lat.fgColorR*255)/31,(lat.fgColorG*255)/31,(lat.fgColorB*255)/31);

		// for sel modes 1 and 2
		int selStartI = 0;
		int selEndI   = -1;
		int selMode   = handle->selectionMode;
		int bufWidth  = handle->bufferWidth;

		int selStartX = handle->selectionStartX;
		int selStartY = handle->selectionStartY;
		int selEndX = handle->selectionEndX;
		int selEndY = handle->selectionEndY;

		if(selEndX < selStartX) std::swap(selStartX,selEndX);
		if(selEndY < selStartY) std::swap(selStartY,selEndY);

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

		bool lastSelected = false;

		SelectObject(hdc,(lat.bold?handle->fontBold:handle->fontNormal));

		SetTextColor(hdc, fColor);
		SetBkColor(hdc, bColor);

		wchar_t* temp = new wchar_t[handle->windowWidth+1];
		for(int y=t;y<b;y++)
		{
			int ay = y+handle->scrollOffsetY;

			int* cRow = handle->characterRows[ay];
			int* aRow = handle->attributeRows[ay];
			int nchars=0;
			int lastx=l;

			for(int x=l;x<r;x++)
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

				if((at.all != lat.all) || (lastSelected != isSelected))
				{
					temp[nchars]=0;
					if(nchars>0)
					{
						ConLibRepaintText(handle, hdc, lastx, y, temp, nchars);
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
				lat=at;
				lastSelected = isSelected;
				if((wchar_t)cRow[ax] != FULLWIDTH_NOSPACE_FILLER)
					temp[nchars++] = (wchar_t)cRow[ax];
			}
			temp[nchars]=0;
			if(nchars>0)
			{
// 				bColor = RGB((lat.bgColorR*255)/31,(lat.bgColorG*255)/31,(lat.bgColorB*255)/31);
// 				fColor = RGB((lat.fgColorR*255)/31,(lat.fgColorG*255)/31,(lat.fgColorB*255)/31);
// 				SetTextColor(hdc, fColor);
// 				SetBkColor(hdc, bColor);
				ConLibRepaintText(handle, hdc, lastx, y, temp, nchars);
			}
		}
		delete[] temp;

		//SetScrollPos(hwnd,SB_VERT,GetScrollPos(hwnd,SB_VERT),TRUE);

		EndPaint(hwnd,&paint);
		break;
	}

	return DefWindowProc(hwnd,message,wParam,lParam);
}

static void ConLibCreateWindow(ConLibHandle handle)
{
	if(!classRegistered)
	{
		WNDCLASSEX wndclass;

		wndclass.cbSize = sizeof(wndclass);
		wndclass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
		wndclass.lpfnWndProc = ConLibWndProc;
		wndclass.cbClsExtra = 0;
		wndclass.cbWndExtra = sizeof(ConLibHandle);
		wndclass.hInstance = GetModuleHandle(NULL);
		wndclass.hIcon = NULL;
		wndclass.hCursor = LoadCursor(NULL, IDC_IBEAM);
		wndclass.hbrBackground = NULL; // CreateSolidBrush(RGB(255,0,255));
		wndclass.lpszMenuName = NULL;
		wndclass.lpszClassName = _T("ConLibWindow");
		wndclass.hIconSm = NULL;

		RegisterClassEx(&wndclass);

		classRegistered = true;
	}

	handle->sWndName = new TCHAR[50];

	swprintf_s(handle->sWndName, 50, _T("ConLibConsole_%08x"), handle->idThread);

	HWND window = CreateWindowEx(
		WS_EX_OVERLAPPEDWINDOW,_T("ConLibWindow"),handle->sWndName, 
		WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL,
		CW_USEDEFAULT, CW_USEDEFAULT, 200, 200,
		0, NULL, GetModuleHandle(NULL), handle);

	handle->fontBold = CreateFont(
		14,7,
		0,0,
		FW_BOLD,
		FALSE,FALSE,FALSE,
		DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
		DEFAULT_QUALITY,FIXED_PITCH|FF_DONTCARE,
		L"Lucida Console"
		);

	ShowWindow(window, SW_NORMAL);

	handle->windowHandle = window;

	// measure the font
	TEXTMETRIC tm;
	//LOGFONT lfont;
	HDC hdc = GetDC(window);
	SelectObject(hdc,handle->fontBold);

	GetTextMetrics(hdc, &tm);

	//GetObject(handle->windowFont, sizeof(LOGFONT), &lfont);
	//lfont.

	handle->characterWidth = tm.tmAveCharWidth;
	handle->characterHeight = tm.tmHeight;

	ReleaseDC(window,hdc);


	handle->fontNormal = CreateFont(
		14,14*handle->characterWidth/handle->characterHeight,
		0,0,
		FW_NORMAL,
		FALSE,FALSE,FALSE,
		DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
		DEFAULT_QUALITY,FIXED_PITCH|FF_DONTCARE,
		L"Lucida Console"
		);


	UpdateWindow(window);

	RedrawWindow(window, NULL, NULL, RDW_FRAME|RDW_INVALIDATE|RDW_ERASE|RDW_FRAME);

	RECT wr;
	RECT cr;

	GetWindowRect(handle->windowHandle,&wr);
	GetClientRect(handle->windowHandle,&cr);

	int xborder = (wr.right - wr.left) - (cr.right - cr.left); // + GetSystemMetrics(SM_CXVSCROLL);
	int yborder = (wr.bottom - wr.top) - (cr.bottom - cr.top);

	int width = -1;
	int height = -1;

	ConLibHandleWindowResize(handle, width, height);

	SetWindowPos(window, 0, 0, 0,
		width + xborder,
		height + yborder,
		SWP_NOMOVE
		);

}

static DWORD ConLibThreadProc(ConLibHandle handle)
{
	ConLibCreateWindow(handle);

	handle->windowCreated = true;
	MSG msg;

	while(GetMessage(&msg, handle->windowHandle, 0, 0))
	{
		bool closing = false;

		if(msg.message==WM_CLOSE)
			closing = true;

		TranslateMessage(&msg);
		DispatchMessage(&msg);

		if(msg.message==0) // why am I getting a WM_NULL message?
			break;

		if((msg.message == WM_DESTROY)|| closing)
		{
			break;
		}
	}

	// it might already be destroyed but anyhow
	DestroyWindow(handle->windowHandle);
	delete handle->sWndName;

	return 0;
}

ConLibHandle CALLBACK ConLibCreateConsole(int bufferWidth, int bufferHeight, int windowWidth, int windowHeight)
{
	ConLibHandle handle = new conLibPrivateData;

	if(!handle)
	{
		return NULL;
	}

	ZeroMemory(handle,sizeof(conLibPrivateData));

	if(bufferWidth < CONSOLE_MIN_BUFFER_WIDTH)		bufferWidth  = CONSOLE_MIN_BUFFER_WIDTH;
	if(bufferHeight < CONSOLE_MIN_BUFFER_HEIGHT)	bufferHeight = CONSOLE_MIN_BUFFER_HEIGHT;
	if(bufferWidth > CONSOLE_MAX_BUFFER_WIDTH)		bufferWidth  = CONSOLE_MAX_BUFFER_WIDTH;
	if(bufferHeight > CONSOLE_MAX_BUFFER_HEIGHT)	bufferHeight = CONSOLE_MAX_BUFFER_HEIGHT;

	if(windowWidth < CONSOLE_MIN_WINDOW_WIDTH)		windowWidth  = CONSOLE_MIN_WINDOW_WIDTH;
	if(windowHeight < CONSOLE_MIN_WINDOW_HEIGHT)	windowHeight = CONSOLE_MIN_WINDOW_HEIGHT;
	if(windowWidth > CONSOLE_MAX_WINDOW_WIDTH)		windowWidth  = CONSOLE_MAX_WINDOW_WIDTH;
	if(windowHeight > CONSOLE_MAX_WINDOW_HEIGHT)	windowHeight = bufferHeight;

	if(windowWidth > bufferWidth)			windowWidth  = bufferWidth;
	if(windowHeight > bufferHeight)			windowHeight = bufferHeight;

	handle->bufferWidth = bufferWidth;
	handle->bufferHeight = bufferHeight;
	handle->windowWidth = windowWidth;
	handle->windowHeight = windowHeight;

	handle->characterBuffer = new int[bufferHeight*bufferWidth];
	handle->attributeBuffer = new int[bufferHeight*bufferWidth];
	handle->characterRows = new int*[bufferHeight];
	handle->attributeRows = new int*[bufferHeight];

	int defAttr = CONSOLE_MAKE_ATTRIBUTE(0,4,31,4,0,4,0);

	for(int y=0;y<bufferHeight;y++)
	{
		handle->characterRows[y] = handle->characterBuffer + (bufferWidth * y);
		handle->attributeRows[y] = handle->attributeBuffer + (bufferWidth * y);

		int* cRow = handle->characterRows[y];
		int* aRow = handle->attributeRows[y];
		for(int x=0;x<handle->bufferWidth;x++)
		{
			cRow[x]=0x20;
			aRow[x]=defAttr;
		}
	}

	handle->defaultAttribute.all = defAttr;
	handle->currentAttribute.all = defAttr;
	handle->scrollOffsetY = 0;
	handle->scrollOffsetX = 0;

	handle->notificationCallback = NULL;

	handle->lastWindowState = SIZE_RESTORED;

	ConLibGotoXY(handle,0,0);

	handle->hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ConLibThreadProc, handle, 0, &handle->idThread);

	while(!handle->windowCreated)
	{
		DWORD code=0;

		if(GetExitCodeThread(handle->hThread, &code))
		{
			if(code != STILL_ACTIVE)
			{
				delete handle;
				return NULL;
			}
		}

		Sleep(0); 
	}

	return handle;
}

void CALLBACK ConLibDestroyConsole(ConLibHandle handle)
{
	if(handle==NULL)
		return;

	SendMessage(handle->windowHandle,WM_CLOSE,0,0);

	if(WaitForSingleObject(handle->hThread,1000)==WAIT_TIMEOUT)
	{
		TerminateThread(handle->hThread, 1);
		DestroyWindow(handle->windowHandle);
	}

	if(handle->characterBuffer) delete handle->characterBuffer;
	if(handle->attributeBuffer) delete handle->attributeBuffer;

	delete handle;
}

void CALLBACK ConLibSetControlParameter(ConLibHandle handle, int parameterId, int value)
{
	switch(parameterId)
	{
	case CONSOLE_CURSOR_X:
		if(value<0)						value=0;
		if(value>=handle->bufferWidth)	value=handle->bufferWidth-1;
		handle->cursorX = value;
		break;
	case CONSOLE_CURSOR_Y:
		if(value<0)						value=0;
		if(value>=handle->bufferHeight)	value=handle->bufferHeight-1;
		handle->cursorY = (value%handle->bufferHeight);
		break;
	case CONSOLE_CURRENT_ATTRIBUTE:
		handle->currentAttribute.all = value;
		break;
	case CONSOLE_SCROLL_OFFSET:
		if(value<0)	value=0;
		if(value>(handle->bufferHeight-handle->windowHeight))
			value=(handle->bufferHeight-handle->windowHeight);
		handle->scrollOffsetY = value;
		break;
	}
}

int  CALLBACK ConLibGetControlParameter(ConLibHandle handle, int parameterId)
{
	switch(parameterId)
	{
	case CONSOLE_CURSOR_X:
		return handle->cursorX;
	case CONSOLE_CURSOR_Y:
		return handle->cursorY;
	case CONSOLE_CURRENT_ATTRIBUTE:
		return handle->currentAttribute.all;
	case CONSOLE_SCROLL_OFFSET:
		return handle->scrollOffsetY;
	}
	return -1;
}

int  CALLBACK ConLibPrintA(ConLibHandle handle, char* text, int length)
{
	COPYDATASTRUCT cd;

	cd.cbData = length;
	cd.lpData = text;
	cd.dwData = CONSOLE_DATA_ANSI;

	return SendMessage(handle->windowHandle, WM_COPYDATA, 0, (LPARAM)&cd);
}

int  CALLBACK ConLibPrintW(ConLibHandle handle, wchar_t* text, int length)
{
	COPYDATASTRUCT cd;

	cd.cbData = length*2;
	cd.lpData = text;
	cd.dwData = CONSOLE_DATA_UNICODE;

	return SendMessage(handle->windowHandle, WM_COPYDATA, 0, (LPARAM)&cd);
}

int CALLBACK ConLibPrintf(ConLibHandle handle, const char* fmt, ...)
{
	va_list lst;

	char text[4096]; // if 4kb is not enough, the code using this needs to be redesigned.

	va_start(lst,fmt);
	int ret = vsprintf_s(text,fmt,lst);
	text[4095]=0;
	va_end(lst);

	if(ret <= 0)
		return ret;

	ret = ConLibPrintA(handle, text, ret);

	return ret;
}

int CALLBACK ConLibWPrintf(ConLibHandle handle, const wchar_t* fmt, ...)
{
	va_list lst;

	wchar_t text[4096]; // if 4k-chars is not enough, the code using this needs to be redesigned.

	va_start(lst,fmt);
	int ret = vswprintf_s(text,fmt,lst);
	text[4095]=0;
	va_end(lst);

	ret = wcslen(text); // 

	ConLibPrintW(handle, text, ret);

	return ret;
}

void CALLBACK ConLibSetWindowTitle(ConLibHandle handle, const wchar_t* windowTitle)
{
	SendMessage(handle->windowHandle,WM_SETTEXT, 0, (LPARAM)windowTitle);
}

void CALLBACK ConLibSetNotificationCallback(ConLibHandle handle, pConLibNotificationCallback callback)
{
	handle->notificationCallback = callback;
}

BOOL APIENTRY DllMain( HMODULE hModule,
					  DWORD  ul_reason_for_call,
					  LPVOID lpReserved
					  )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

