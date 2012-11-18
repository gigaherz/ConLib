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

#define CLMSG_NOOP (WM_USER+0)
#define CLMSG_CLEAR (WM_USER+1)

#define TAB_SIZE 8
#define FULLWIDTH_NOSPACE_FILLER 0x2060

bool classRegistered=false;

MSG __declspec(thread) messageToPost;

LONG timerNextID=0;

void swap(int* a, int* b)
{
	int t = *a;
	*a = *b;
	*b = t;
}

static void clRepaintText(ConLibHandle handle, HDC hdc, int x, int y, wchar_t* text, int nchars)
{
	TextOut(hdc,x * handle->characterWidth, y*handle->characterHeight, text, nchars);
}

static void clUpdateScrollBars(ConLibHandle handle)
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

static int clInternalWrite(ConLibHandle handle, const wchar_t* data, int characters)
{
	int chars;

	for(chars=characters;chars>0;chars--)
	{
		wchar_t ch = *data++;
		clPutChar(handle, ch);
		if(IsFullWidth(ch))
			clPutChar(handle, FULLWIDTH_NOSPACE_FILLER);
	}

//	if(characters>0)
//		InvalidateRect(handle->windowHandle,NULL, FALSE);

	return characters;
}

static int clCopyDataHandler(ConLibHandle handle, COPYDATASTRUCT* cds)
{
	if(cds->dwData == CONSOLE_DATA_ANSI)
	{
		int ret;
		wchar_t *data = (wchar_t*)malloc(cds->cbData*4);

		int nch = MultiByteToWideChar(CP_THREAD_ACP, MB_PRECOMPOSED, (LPCSTR)cds->lpData, cds->cbData, data, cds->cbData*2);
		data[nch]=0;

		ret = clInternalWrite(handle,data,nch);

		free(data);

		return ret;
	}
	else if(cds->dwData == CONSOLE_DATA_UNICODE)
	{
		return clInternalWrite(handle,(wchar_t*)cds->lpData,cds->cbData>>1);
	}
	return -1;
}

static void clHandleWindowResize(ConLibHandle handle, int* width, int* height)
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

static void clProcessResize(ConLibHandle handle, RECT* r, int wParam)
{
	//ConLibUpdateScrollBars(handle);

	int xborder, yborder, yhscroll, cwidth, cheight;
	int width, height;
	bool hadHS, newHS;

	RECT wr,cr;
	if(GetWindowRect(handle->windowHandle,&wr)==0) return;
	if(GetClientRect(handle->windowHandle,&cr)==0) return;

	xborder = (wr.right - wr.left) - (cr.right - cr.left);
	yborder = (wr.bottom - wr.top) - (cr.bottom - cr.top);

	yhscroll = GetSystemMetrics(SM_CYHSCROLL);

	cwidth = (r->right - r->left) - xborder;
	cheight = (r->bottom - r->top) - yborder;

	width = cwidth + (handle->characterWidth / 2);
	height = cheight + (handle->characterHeight / 2);

	hadHS = (handle->windowWidth == handle->bufferWidth);
	//hadVS = (handle->windowHeight == handle->windowHeight);

	clHandleWindowResize(handle, &width, &height);
	clUpdateScrollBars(handle);

	newHS = (handle->windowWidth == handle->bufferWidth);
	//bool newVS = (handle->windowHeight == handle->windowHeight);

	if(hadHS && !newHS) // we don't need the HS anymore
	{
		height += yhscroll;
		yborder -= yhscroll; // compensate border differences

		clHandleWindowResize(handle, &width, &height);
	}

	if(!hadHS && newHS) // added a scrollbar
	{
		height -= handle->characterHeight * (yhscroll / handle->characterHeight); 
		yborder += yhscroll; // compensate border differences

		clHandleWindowResize(handle, &width, &height);
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

static void clRepaintArea(ConLibHandle handle, int sx, int sy, int ex, int ey)
{
	RECT r;
	
	r.left = (sx - handle->scrollOffsetX) * handle->characterWidth;
	r.top = (sy - handle->scrollOffsetY) * handle->characterHeight;
	r.right = (ex + 2 - handle->scrollOffsetX) * handle->characterWidth;
	r.bottom = (ey + 2 - handle->scrollOffsetY) * handle->characterHeight;

	InvalidateRect(handle->windowHandle,&r,TRUE);
}

void clTimedPostMessage(ConLibHandle handle, int delay, HWND hwnd, int msg, WPARAM wParam, LPARAM lParam)
{
	LONG id = InterlockedExchangeAdd(&timerNextID,1);

	messageToPost.hwnd = hwnd;
	messageToPost.message = msg;
	messageToPost.wParam = wParam;
	messageToPost.lParam = lParam;

	SetTimer(hwnd,handle->idThread,500,NULL);
};

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

static void clClearArea(ConLibHandle handle, int mode) 
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

static LRESULT CALLBACK clWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
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
		return clCopyDataHandler(handle, (COPYDATASTRUCT*)lParam);

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
		return clWndProc(messageToPost.hwnd,messageToPost.message,messageToPost.wParam,messageToPost.lParam);

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
					clTimedPostMessage(handle, 200, hwnd, WM_MOUSEMOVE, wParam,lParam);
			}

			if(handle->selectionEndX >= (handle->scrollOffsetX+handle->windowWidth))
			{
				handle->scrollOffsetX = handle->selectionEndX - handle->windowWidth + 1;

				if((handle->scrollOffsetX + handle->windowWidth) > handle->bufferWidth)
					handle->scrollOffsetX = handle->bufferWidth - handle->windowWidth + 1;
				else
					clTimedPostMessage(handle, 200, hwnd, WM_MOUSEMOVE, wParam,lParam);
			}

			if(handle->selectionEndY < handle->scrollOffsetY)
			{
				handle->scrollOffsetY = handle->selectionEndY;

				if(handle->scrollOffsetY < 0)
					handle->scrollOffsetY = 0;
				else
					clTimedPostMessage(handle, 200, hwnd, WM_MOUSEMOVE, wParam,lParam);
			}

			if(handle->selectionEndY >= (handle->scrollOffsetY+handle->windowHeight))
			{
				handle->scrollOffsetY = handle->selectionEndY - handle->windowHeight + 1;

				if((handle->scrollOffsetY + handle->windowHeight) > handle->bufferHeight)
					handle->scrollOffsetY = handle->bufferHeight - handle->windowHeight + 1;
				else
					clTimedPostMessage(handle, 200, hwnd, WM_MOUSEMOVE, wParam,lParam);
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
			int x, y;

			wchar_t* copiedText = (wchar_t*)malloc(sizeof(wchar_t) * handle->bufferWidth * (handle->selectionEndY - handle->selectionStartY + 1));
			int copiedTextLength = 0;

			switch(handle->selectionMode)
			{
			case 1: // inline
				{
					for(x=handle->selectionStartX;x<handle->bufferWidth;x++)
					{
						copiedText[copiedTextLength++] = (wchar_t)(handle->characterRows[handle->selectionStartY][x]);
					}
					if (handle->selectionStartY < handle->selectionEndY)
					{
						copiedText[copiedTextLength++] = L'\r';
						copiedText[copiedTextLength++] = L'\n';
						for(y=handle->selectionStartY+1;y<handle->selectionEndY;y++)
						{
							for(x=0;x<handle->bufferWidth;x++)
							{
								copiedText[copiedTextLength++] = (wchar_t)(handle->characterRows[y][x]);
							}
							copiedText[copiedTextLength++] = L'\r';
							copiedText[copiedTextLength++] = L'\n';
						}
						for(x=0;x<=handle->selectionEndX;x++)
						{
							copiedText[copiedTextLength++] = (wchar_t)(handle->characterRows[handle->selectionEndY][x]);
						}
						if(handle->selectionEndX+1 == handle->bufferWidth)
						{
							copiedText[copiedTextLength++] = L'\r';
							copiedText[copiedTextLength++] = L'\n';
						}
					}
				}
				break;
			case 2: // whole lines
				{
					for(y=handle->selectionStartY;y<=handle->selectionEndY;y++)
					{
						for(x=0;x<handle->bufferWidth;x++)
						{
							copiedText[copiedTextLength++] = (wchar_t)(handle->characterRows[y][x]);
						}
						copiedText[copiedTextLength++] = L'\r';
						copiedText[copiedTextLength++] = L'\n';
					}
				}
				break;
			case 3: // rectangle
				{
					for(y=handle->selectionStartY;y<=handle->selectionEndY;y++)
					{
						for(x=handle->selectionStartY;x<=handle->selectionEndY;x++)
						{
							copiedText[copiedTextLength++] = (wchar_t)(handle->characterRows[y][x]);
						}
						copiedText[copiedTextLength++] = L'\r';
						copiedText[copiedTextLength++] = L'\n';
					}
				}
				break;
			}

			if(copiedTextLength > 0)
			{
				if(OpenClipboard(hwnd))
				{
					HANDLE hMem;

					EmptyClipboard();

					if(hMem = GlobalAlloc(GMEM_MOVEABLE, (copiedTextLength+1) * sizeof(wchar_t)))
					{
						wchar_t* ptr = (wchar_t*)GlobalLock(hMem);
						CopyMemory(ptr,copiedText,(copiedTextLength+1) * sizeof(wchar_t));
						ptr[copiedTextLength]=0;
						GlobalUnlock(hMem);

						SetClipboardData(CF_UNICODETEXT,hMem);
					}

					if(hMem = GlobalAlloc(GMEM_MOVEABLE, copiedTextLength+1))
					{
						char* ptr = (char*)GlobalLock(hMem);
						WideCharToMultiByte(CP_THREAD_ACP,WC_COMPOSITECHECK,copiedText,copiedTextLength,ptr,copiedTextLength+1,NULL,NULL);
						ptr[copiedTextLength]=0;
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
			RECT wr,cr;

			int xborder, yborder, maxx, maxy, cxm, cym;

			MINMAXINFO *mmi=(MINMAXINFO*)lParam;

			if(!handle)
				break;

			// make sure scrollbar state is updated
			clUpdateScrollBars(handle);

			if(GetWindowRect(handle->windowHandle,&wr)==0) break;
			if(GetClientRect(handle->windowHandle,&cr)==0) break;

			xborder = (wr.right - wr.left) - (cr.right - cr.left);
			yborder = (wr.bottom - wr.top) - (cr.bottom - cr.top);

			maxx = xborder + (handle->bufferWidth * handle->characterWidth);
			maxy = yborder + (handle->bufferHeight * handle->characterHeight);

			cxm = handle->characterWidth * (GetSystemMetrics(SM_CXMAXIMIZED)/handle->characterWidth);
			cym = handle->characterHeight * (GetSystemMetrics(SM_CYMAXIMIZED)/handle->characterHeight);

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

			clProcessResize(handle, r, wParam);
		}

		break;

	case WM_SIZE:

		if( (wParam != SIZE_MINIMIZED) && (handle->lastWindowState != wParam))
		{
			RECT r;

			if(GetWindowRect(hwnd,&r)==0) break;

			clProcessResize(handle, &r, 0);

			clUpdateScrollBars(handle);

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
		{
			HDC hdc;
			PAINTSTRUCT paint;
			COLORREF bColor, fColor;
			int l,r,t,b;
			int selStartI, selEndI, selMode, bufWidth;
			int selStartX, selStartY, selEndX, selEndY;
			bool lastSelected;
			wchar_t* temp;
			int y, x;
			charAttribute lat;

			clUpdateScrollBars(handle);

			hdc = BeginPaint(hwnd,&paint);

			if(hdc==NULL)
				break;

			l = paint.rcPaint.left/handle->characterWidth;
			r = (paint.rcPaint.right+handle->characterWidth-1)/handle->characterWidth;

			t = paint.rcPaint.top/handle->characterHeight;
			b = (paint.rcPaint.bottom+handle->characterHeight-1)/handle->characterHeight;

			if(r > handle->windowWidth)
				r=handle->windowWidth;
			if(b > handle->windowHeight)
				b=handle->windowHeight;

			lat=handle->currentAttribute;

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

				for(x=l;x<r;x++)
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
					clRepaintText(handle, hdc, lastx, y, temp, nchars);
				}
			}
			free(temp);

			//SetScrollPos(hwnd,SB_VERT,GetScrollPos(hwnd,SB_VERT),TRUE);

			EndPaint(hwnd,&paint);
		}
		break;
	case CLMSG_CLEAR:
		clClearArea(handle, lParam);
		break;
	}

	return DefWindowProc(hwnd,message,wParam,lParam);
}

static void clCreateWindow(ConLibHandle handle)
{
	HWND window;
	TEXTMETRIC tm;
	HDC hdc;
	RECT wr, cr;
	int xborder, yborder, width, height;

	if(!classRegistered)
	{
		WNDCLASSEX wndclass;

		wndclass.cbSize = sizeof(wndclass);
		wndclass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
		wndclass.lpfnWndProc = clWndProc;
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

	handle->sWndName = (wchar_t*)malloc(sizeof(wchar_t) * 50);

	swprintf_s(handle->sWndName, 50, _T("ConLibConsole_%08x"), handle->idThread);

	window = CreateWindowEx(
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
	//LOGFONT lfont;
	hdc = GetDC(window);
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
	
	GetWindowRect(handle->windowHandle,&wr);
	GetClientRect(handle->windowHandle,&cr);

	xborder = (wr.right - wr.left) - (cr.right - cr.left); // + GetSystemMetrics(SM_CXVSCROLL);
	yborder = (wr.bottom - wr.top) - (cr.bottom - cr.top);

	width = -1;
	height = -1;

	clHandleWindowResize(handle, &width, &height);

	SetWindowPos(window, 0, 0, 0,
		width + xborder,
		height + yborder,
		SWP_NOMOVE
		);
}

static DWORD clThreadProc(ConLibHandle handle)
{
	clCreateWindow(handle);

	handle->windowCreated = true;

	while(true)
	{
		MSG msg;

		if(PeekMessage(&msg, handle->windowHandle, 0, 0, PM_REMOVE))
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
		else
		{
			DWORD ret = MsgWaitForMultipleObjectsEx(ARRAYSIZE(handle->handles), handle->handles, INFINITE, QS_ALLINPUT, 0);
			if(ret >= WAIT_OBJECT_0 && ret < (WAIT_OBJECT_0+ARRAYSIZE(handle->handles)))
			{
				int which = ret - WAIT_OBJECT_0;					
			}
		}
	}

	// it might already be destroyed but anyhow
	DestroyWindow(handle->windowHandle);
	free(handle->sWndName);

	return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ConLibHandle CALLBACK ConLibCreateConsole(int bufferWidth, int bufferHeight, int windowWidth, int windowHeight, int defAttr)
{
	int y, x;

	ConLibHandle handle = (ConLibHandle)malloc(sizeof(struct conLibPrivateData));

	if(!handle)
	{
		return NULL;
	}

	ZeroMemory(handle, sizeof(struct conLibPrivateData));

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

	handle->characterBuffer = (int*)malloc(sizeof(int) * (bufferHeight*bufferWidth));
	handle->attributeBuffer = (int*)malloc(sizeof(int) * (bufferHeight*bufferWidth));
	handle->characterRows = (int**)malloc(sizeof(int*) * (bufferHeight));
	handle->attributeRows = (int**)malloc(sizeof(int*) * (bufferHeight));

	//int defAttr = CONSOLE_MAKE_ATTRIBUTE(0,4,31,4,0,4,0);

	for(y=0;y<bufferHeight;y++)
	{
		int* cRow = handle->characterRows[y] = handle->characterBuffer + (bufferWidth * y);
		int* aRow = handle->attributeRows[y] = handle->attributeBuffer + (bufferWidth * y);

		for(x=0;x<handle->bufferWidth;x++)
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

	handle->hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)clThreadProc, handle, 0, &handle->idThread);

	while(!handle->windowCreated)
	{
		DWORD code=0;

		if(GetExitCodeThread(handle->hThread, &code))
		{
			if(code != STILL_ACTIVE)
			{
				free(handle);
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

	if(handle->characterBuffer) free(handle->characterBuffer);
	if(handle->attributeBuffer) free(handle->attributeBuffer);

	free(handle);
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
	case CONSOLE_DEFAULT_ATTRIBUTE:
		handle->defaultAttribute.all = value;
		break;
	case CONSOLE_SCROLL_OFFSET_X:
		if(value<0)	value=0;
		if(value>(handle->bufferWidth-handle->windowWidth))
			value=(handle->bufferWidth-handle->windowWidth);
		handle->scrollOffsetX = value;
		break;
	case CONSOLE_SCROLL_OFFSET_Y:
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
	case CONSOLE_DEFAULT_ATTRIBUTE:
		return handle->defaultAttribute.all;
	case CONSOLE_SCROLL_OFFSET_X:
		return handle->scrollOffsetX;
	case CONSOLE_SCROLL_OFFSET_Y:
		return handle->scrollOffsetY;
	case CONSOLE_BUFFER_SIZE_X:
		return handle->bufferWidth;
	case CONSOLE_BUFFER_SIZE_Y:
		return handle->bufferHeight;
	case CONSOLE_WINDOW_SIZE_X:
		return handle->windowWidth;
	case CONSOLE_WINDOW_SIZE_Y:
		return handle->windowHeight;
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
	int ret, len;
	va_list lst;
	char *text;
		
	va_start(lst,fmt);
	len = _vsnprintf(NULL, 0, fmt, lst);
	va_end(lst);
	
	text = (char*)malloc(sizeof(char) * (len+1));

	va_start(lst,fmt);
	ret = _vsnprintf(text, len, fmt,lst);
	text[len]=0;
	va_end(lst);

	ret = strlen(text); // 

	ConLibPrintA(handle, text, ret);

	return ret;
}

int CALLBACK ConLibWPrintf(ConLibHandle handle, const wchar_t* fmt, ...)
{
	int ret, len;
	va_list lst;
	wchar_t *text;
		
	va_start(lst,fmt);
	len = _vsnwprintf(NULL, 0, fmt, lst);
	va_end(lst);
	
	text = (wchar_t*)malloc(sizeof(wchar_t) * (len+1));

	va_start(lst,fmt);
	ret = _vsnwprintf(text, len, fmt,lst);
	text[len]=0;
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

void CALLBACK ConLibClearBuffer(ConLibHandle handle)
{
	SendMessage(handle->windowHandle, CLMSG_CLEAR, 0, 0);
}

void CALLBACK ConLibClearVisibleArea(ConLibHandle handle)
{
	SendMessage(handle->windowHandle, CLMSG_CLEAR, 0, 1);
}

void CALLBACK ConLibClearLine(ConLibHandle handle)
{
	SendMessage(handle->windowHandle, CLMSG_CLEAR, 0, 2);
}

pvoid CALLBACK ConLibSetIOHandle(ConLibHandle handle, int nHandle, pvoid win32FileHandle)
{
	HANDLE ret;

	if(nHandle >=3)
		return INVALID_HANDLE_VALUE;

	ret = handle->handles[nHandle];
	handle->handles[nHandle] = win32FileHandle;
	PostMessage(handle->windowHandle, CLMSG_NOOP, 0, 0); // wake up the message pump so it notices the change of handle

	return ret;
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
