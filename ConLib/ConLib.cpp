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
#include "stdafx.h"

#include <tchar.h>
#include <stdarg.h>
#include <stdio.h>
#include "ConLibInternal.h"
#include <windowsx.h>

#define TAB_SIZE 8

bool classRegistered=false;

static void clRepaintText(clHandle handle, HDC hdc, int x, int y, wchar_t* text, int nchars)
{
	TextOut(hdc,x * handle->characterWidth, y*handle->characterHeight, text, nchars);
}

/*static void clUpdateLine(clHandle handle)
{
	RECT r = {
		0,
		(handle->cursorY-handle->scrollOffset)*handle->characterHeight,
		handle->windowWidth * handle->characterWidth,
		(handle->cursorY+1-handle->scrollOffset)*handle->characterHeight
	};

	InvalidateRect(handle->windowHandle,&r,FALSE);
}*/

static void clScrollBufferDown(clHandle handle)
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

static void clMoveDown(clHandle handle)
{
	handle->cursorY++;
	if(handle->cursorY>=handle->bufferHeight)
	{
		handle->cursorY--;
		clScrollBufferDown(handle);

		handle->scrollOffset = handle->bufferHeight-handle->windowHeight;
		SetScrollPos(handle->windowHandle,SB_VERT,handle->scrollOffset,TRUE);
		
		ScrollWindowEx(handle->windowHandle,0,-handle->characterHeight,NULL,NULL,NULL,NULL,SW_INVALIDATE);
	}

	if(handle->cursorY < handle->scrollOffset)
	{
		int scrollDifference = handle->scrollOffset-handle->cursorY;

		if((-scrollDifference)>=handle->windowHeight)
			InvalidateRect(handle->windowHandle,NULL,FALSE);
		else
			ScrollWindowEx(handle->windowHandle,0,handle->characterHeight*scrollDifference,NULL,NULL,NULL,NULL,SW_INVALIDATE);

		handle->scrollOffset = handle->cursorY;
		SetScrollPos(handle->windowHandle,SB_VERT,handle->scrollOffset,TRUE);
	}

	if(handle->cursorY >= (handle->scrollOffset+handle->windowHeight))
	{
		int scrollDifference = (handle->cursorY - (handle->scrollOffset+handle->windowHeight-1));

		if(scrollDifference>=handle->windowHeight)
			InvalidateRect(handle->windowHandle,NULL,FALSE);
		else
			ScrollWindowEx(handle->windowHandle,0,-handle->characterHeight*scrollDifference,NULL,NULL,NULL,NULL,SW_INVALIDATE);

		handle->scrollOffset = handle->cursorY - handle->windowHeight + 1;
		SetScrollPos(handle->windowHandle,SB_VERT,handle->scrollOffset,TRUE);
	}

	if(handle->scrollOffset<0)
		handle->scrollOffset=0;
}

static void clMoveNext(clHandle handle)
{
	handle->cursorX++;
	if(handle->cursorX>=handle->bufferWidth)
	{
		handle->cursorX=0;
		clMoveDown(handle);
	}
}

static void clPutChar(clHandle handle, wchar_t chr)
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
		cRow[handle->cursorX] = chr;
		aRow[handle->cursorX] = handle->currentAttribute.all;

		RECT r = {
			handle->cursorX * handle->characterWidth,
			(handle->cursorY-handle->scrollOffset)*handle->characterHeight,
			(handle->cursorX+1) * handle->characterWidth,
			(handle->cursorY+1-handle->scrollOffset)*handle->characterHeight
		};

		InvalidateRect(handle->windowHandle,&r,FALSE);

		clMoveNext(handle);
	}
}

static int clInternalWrite(clHandle handle, const wchar_t* data, int characters)
{
	int chars=characters;
	for(int chars=characters;chars>0;chars--)
	{
		clPutChar(handle, *data++);
	}
//	if(characters>0)
//		InvalidateRect(handle->windowHandle,NULL, FALSE);

	return characters;
}

static int clCopyDataHandler(clHandle handle, COPYDATASTRUCT* cds)
{
	if(cds->dwData == CL_DATA_ANSI)
	{
		wchar_t data[4096];
		MultiByteToWideChar(CP_THREAD_ACP, MB_PRECOMPOSED, (LPCSTR)cds->lpData, cds->cbData, data, 4095);
		data[4095]=0;
		data[cds->cbData]=0;

		return clInternalWrite(handle,data,cds->cbData);
	}
	else if(cds->dwData == CL_DATA_UNICODE)
	{
		return clInternalWrite(handle,(wchar_t*)cds->lpData,cds->cbData>>1);
	}
	return -1;
}

static void clHandleWindowResize(clHandle handle, int& width, int& height)
{
	int w = width;
	int h = height;

	int sw = w / handle->characterWidth;
	int sh = h / handle->characterHeight;

	if(sw < CL_MIN_WINDOW_WIDTH)	sw = CL_MIN_WINDOW_WIDTH;
	if(sh < CL_MIN_WINDOW_HEIGHT)	sh = CL_MIN_WINDOW_HEIGHT;
	if(sw > CL_MAX_WINDOW_WIDTH)	sw = CL_MAX_WINDOW_WIDTH;
	if(sh > CL_MAX_WINDOW_HEIGHT)	sh = CL_MAX_WINDOW_HEIGHT;

	if(sw > handle->bufferWidth)	sw = handle->bufferWidth;
	if(sh > handle->bufferHeight)	sh = handle->bufferHeight;

	if(width == -1)		sw = handle->windowWidth;
	if(height == -1)	sh = handle->windowHeight;

	width = sw * handle->characterWidth;
	height = sh * handle->characterHeight;

	handle->windowWidth = sw;
	handle->windowHeight = sh;

}

static LRESULT CALLBACK clWndProc(HWND hwnd, UINT message,
								WPARAM wParam, LPARAM lParam)
{
	clHandle handle = (clHandle)GetWindowLongPtr(hwnd, GWL_USERDATA);
	switch(message)
	{
	case WM_COPYDATA:
		return clCopyDataHandler(handle, (COPYDATASTRUCT*)lParam);

	case WM_SIZING:

		if(wParam != SIZE_MINIMIZED)
		{
			RECT* r = (RECT*)lParam;

			RECT wr,cr;
			if(GetWindowRect(handle->windowHandle,&wr)==0) break;
			if(GetClientRect(handle->windowHandle,&cr)==0) break;

			int xborder = (wr.right - wr.left) - (cr.right - cr.left); // + GetSystemMetrics(SM_CXVSCROLL);
			int yborder = (wr.bottom - wr.top) - (cr.bottom - cr.top);

			int owidth = (r->right - r->left) - xborder;
			int oheight = (r->bottom - r->top) - yborder;

			int width = owidth;
			int height = oheight;

			clHandleWindowResize(handle, width,height);

			lParam = (height << 16) | width;

			if(width != owidth)
			{
				if( (wParam == WMSZ_LEFT) || (wParam == WMSZ_TOPLEFT) || (wParam == WMSZ_BOTTOMLEFT)  ) // left
				{
					r->left = r->right - (width + xborder);
				}
				else if( (wParam == WMSZ_RIGHT) || (wParam == WMSZ_TOPRIGHT) || (wParam == WMSZ_BOTTOMRIGHT)  ) // right
				{
					r->right = r->left + (width + xborder);
				}
			}

			if(height != oheight)
			{
				if( (wParam == WMSZ_TOP) || (wParam == WMSZ_TOPLEFT) || (wParam == WMSZ_TOPRIGHT)  ) // top
				{
					r->top = r->bottom - (height + yborder);
				}	
				else if( (wParam == WMSZ_BOTTOM) || (wParam == WMSZ_BOTTOMLEFT) || (wParam == WMSZ_BOTTOMRIGHT)  ) // bottom
				{
					r->bottom = r->top + (height + yborder);
				}
			}
		}

		break;
	case WM_VSCROLL:
		{
			SCROLLINFO si;
			si.cbSize = sizeof(si);
			si.fMask = SIF_TRACKPOS;
			GetScrollInfo(hwnd, SB_VERT, &si);
			int sbValue = si.nTrackPos;
			if(sbValue != handle->scrollOffset)
			{
				if(sbValue > (handle->bufferHeight - handle->windowHeight))
					sbValue = (handle->bufferHeight - handle->windowHeight);

				if(sbValue < 0)
					sbValue = 0;

				handle->scrollOffset = sbValue;
				SetScrollPos(hwnd,SB_VERT,sbValue,TRUE);
				InvalidateRect(hwnd,NULL,FALSE);
			}
		}
		return 0;
	case WM_PAINT:

		PAINTSTRUCT paint;

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

		SelectObject(hdc,handle->windowFont);

		charAttribute lat=handle->currentAttribute;

		COLORREF bColor = RGB((lat.bgColorR*255)/31,(lat.bgColorG*255)/31,(lat.bgColorB*255)/31);
		COLORREF fColor = RGB((lat.fgColorR*255)/31,(lat.fgColorG*255)/31,(lat.fgColorB*255)/31);

		SetTextColor(hdc, fColor);
		SetBkColor(hdc, bColor);

		wchar_t* temp = new wchar_t[handle->windowWidth+1];
		for(int y=t;y<b;y++)
		{
			int* cRow = handle->characterRows[y+handle->scrollOffset];
			int* aRow = handle->attributeRows[y+handle->scrollOffset];
			int nchars=0;
			int lastx=l;

			for(int x=l;x<r;x++)
			{
				charAttribute at;

				at.all = aRow[x];

				if(at.all != lat.all)
				{
					temp[nchars]=0;
					if(nchars>0)
					{
						clRepaintText(handle, hdc, lastx, y, temp, nchars);
					}
					nchars=0;
					lastx=x;
					lat=at;
					bColor = RGB((lat.bgColorR*255)/31,(lat.bgColorG*255)/31,(lat.bgColorB*255)/31);
					fColor = RGB((lat.fgColorR*255)/31,(lat.fgColorG*255)/31,(lat.fgColorB*255)/31);
					SetTextColor(hdc, fColor);
					SetBkColor(hdc, bColor);
				}
				lat=at;
				temp[nchars++] = (wchar_t)cRow[x];
			}
			temp[nchars]=0;
			if(nchars>0)
			{
				bColor = RGB((lat.bgColorR*255)/31,(lat.bgColorG*255)/31,(lat.bgColorB*255)/31);
				fColor = RGB((lat.fgColorR*255)/31,(lat.fgColorG*255)/31,(lat.fgColorB*255)/31);
				SetTextColor(hdc, fColor);
				SetBkColor(hdc, bColor);
				clRepaintText(handle, hdc, lastx, y, temp, nchars);
			}
		}
		delete[] temp;

		//SetScrollPos(hwnd,SB_VERT,GetScrollPos(hwnd,SB_VERT),TRUE);

		EndPaint(hwnd,&paint);
		break;
	}

	return DefWindowProc(hwnd,message,wParam,lParam);
}

static void clCreateWindow(clHandle handle)
{
	if(!classRegistered)
	{
		WNDCLASSEX wndclass;

		wndclass.cbSize = sizeof(wndclass);
		wndclass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
		wndclass.lpfnWndProc = clWndProc;
		wndclass.cbClsExtra = 0;
		wndclass.cbWndExtra = sizeof(clHandle);
		wndclass.hInstance = GetModuleHandle(NULL);
		wndclass.hIcon = NULL;
		wndclass.hCursor = LoadCursor(NULL, IDC_IBEAM);
		wndclass.hbrBackground = CreateSolidBrush(RGB(255,0,255));
		wndclass.lpszMenuName = NULL;
		wndclass.lpszClassName = _T("ConLibWindow");
		wndclass.hIconSm = NULL;

		RegisterClassEx(&wndclass);

		classRegistered = true;
	}

	TCHAR sWndName[2000];

	swprintf_s(sWndName, 2000, _T("ConLibConsole_%08x"), handle);

	HWND window = CreateWindowEx(
		WS_EX_OVERLAPPEDWINDOW,_T("ConLibWindow"),sWndName, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_VSCROLL,
		CW_USEDEFAULT, CW_USEDEFAULT, 200, 200,
		0, 0, 0, 0);

	handle->windowFont = CreateFont(
		16,0,
		0,0,
		FW_DONTCARE,
		FALSE,FALSE,FALSE,
		ANSI_CHARSET,
		OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
		DEFAULT_QUALITY,FIXED_PITCH|FF_DONTCARE,
		L"Lucida Console"
		);

	SetWindowLongPtr(window, GWL_USERDATA, (LONG_PTR)handle);
	ShowWindow(window, SW_NORMAL);

	handle->windowHandle = window;

	// measure the font
	TEXTMETRIC tm;
	//LOGFONT lfont;
	HDC hdc = GetDC(window);
	SelectObject(hdc,handle->windowFont);

	GetTextMetrics(hdc, &tm);

	//GetObject(handle->windowFont, sizeof(LOGFONT), &lfont);
	//lfont.

	handle->characterWidth = tm.tmAveCharWidth;
	handle->characterHeight = tm.tmHeight;

	SCROLLINFO si;

	si.cbSize = sizeof(SCROLLINFO);
	si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;
	si.nPage = handle->windowHeight;
	si.nMin = 0;
	si.nMax = (handle->bufferHeight-handle->windowHeight);
	si.nPos = 0;
	
	SetScrollInfo(window,SB_VERT, &si, TRUE);

	ReleaseDC(window,hdc);

	UpdateWindow(window);

	//SendMessage(window,SBM_ENABLE_ARROWS,ESB_ENABLE_BOTH,0);

	RedrawWindow(window, NULL, NULL, RDW_FRAME|RDW_INVALIDATE|RDW_ERASE|RDW_FRAME);

	RECT wr;
	RECT cr;

	GetWindowRect(handle->windowHandle,&wr);
	GetClientRect(handle->windowHandle,&cr);

	int xborder = (wr.right - wr.left) - (cr.right - cr.left); // + GetSystemMetrics(SM_CXVSCROLL);
	int yborder = (wr.bottom - wr.top) - (cr.bottom - cr.top);

	int width = -1;
	int height = -1;

	clHandleWindowResize(handle, width, height);

	SetWindowPos(window, 0, 0, 0,
		width + xborder,
		height + yborder,
		SWP_NOMOVE
		);

}

static DWORD clThreadProc(clHandle handle)
{
	clCreateWindow(handle);

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
			break;
	}

	// it might already be destroyed but anyhow
	DestroyWindow(handle->windowHandle);

	return 0;
}

clHandle clCreateConsole(int bufferWidth, int bufferHeight, int windowWidth, int windowHeight)
{
	clHandle handle = new conLibPrivateData;

	ZeroMemory(handle,sizeof(conLibPrivateData));

	if(bufferWidth < CL_MIN_BUFFER_WIDTH)	bufferWidth  = CL_MIN_BUFFER_WIDTH;
	if(bufferHeight < CL_MIN_BUFFER_HEIGHT)	bufferHeight = CL_MIN_BUFFER_HEIGHT;
	if(bufferWidth > CL_MAX_BUFFER_WIDTH)	bufferWidth  = CL_MAX_BUFFER_WIDTH;
	if(bufferHeight > CL_MAX_BUFFER_HEIGHT)	bufferHeight = CL_MAX_BUFFER_HEIGHT;

	if(windowWidth < CL_MIN_WINDOW_WIDTH)	windowWidth  = CL_MIN_WINDOW_WIDTH;
	if(windowHeight < CL_MIN_WINDOW_HEIGHT)	windowHeight = CL_MIN_WINDOW_HEIGHT;
	if(windowWidth > CL_MAX_WINDOW_WIDTH)	windowWidth  = CL_MAX_WINDOW_WIDTH;
	if(windowHeight > CL_MAX_WINDOW_HEIGHT)	windowHeight = bufferHeight;

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

	int defAttr = CL_MAKE_ATTRIBUTE(0,4,31,4,0,4,0);

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
	handle->scrollOffset = 0;

	clGotoXY(handle,0,0);

	handle->hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)clThreadProc, handle, 0, &handle->idThread);

	while(!handle->windowCreated) Sleep(0); 

	return handle;
}

void clDestroyConsole(clHandle handle)
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

void clSetControlParameter(clHandle handle, int parameterId, int value)
{
	switch(parameterId)
	{
	case CL_CURSOR_X:
		if(value<0)						value=0;
		if(value>=handle->bufferWidth)	value=handle->bufferWidth-1;
		handle->cursorX = value;
		break;
	case CL_CURSOR_Y:
		if(value<0)						value=0;
		if(value>=handle->bufferHeight)	value=handle->bufferHeight-1;
		handle->cursorY = (value%handle->bufferHeight);
		break;
	case CL_CURRENT_ATTRIBUTE:
		handle->currentAttribute.all = value;
		break;
	case CL_SCROLL_OFFSET:
		if(value<0)	value=0;
		if(value>(handle->bufferHeight-handle->windowHeight))
			value=(handle->bufferHeight-handle->windowHeight);
		handle->scrollOffset = value;
		break;
	}
}

int  clGetControlParameter(clHandle handle, int parameterId)
{
	switch(parameterId)
	{
	case CL_CURSOR_X:
		return handle->cursorX;
	case CL_CURSOR_Y:
		return handle->cursorY;
	case CL_CURRENT_ATTRIBUTE:
		return handle->currentAttribute.all;
	case CL_SCROLL_OFFSET:
		return handle->scrollOffset;
	}
	return -1;
}

int  clPrintA(clHandle console, char* text, int length)
{
	COPYDATASTRUCT cd;

	cd.cbData = length;
	cd.lpData = text;
	cd.dwData = CL_DATA_ANSI;

	return SendMessage(console->windowHandle, WM_COPYDATA, (WPARAM)console, (LPARAM)&cd);
}

int  clPrintW(clHandle console, wchar_t* text, int length)
{
	COPYDATASTRUCT cd;

	cd.cbData = length;
	cd.lpData = text;
	cd.dwData = CL_DATA_UNICODE;

	return SendMessage(console->windowHandle, WM_COPYDATA, (WPARAM)console, (LPARAM)&cd);
}

int clPrintf(clHandle handle, const char* fmt, ...)
{
	va_list lst;

	char text[4096]; // if 4kb is not enough, the code using this needs to be redesigned.

	va_start(lst,fmt);
	int ret = vsprintf_s(text,fmt,lst);
	text[4095]=0;
	va_end(lst);

	if(ret <= 0)
		return ret;

	ret = clPrintA(handle, text, ret);

	return ret;
}

int clWPrintf(clHandle handle, const wchar_t* fmt, ...)
{
	va_list lst;

	wchar_t text[4096]; // if 4k-chars is not enough, the code using this needs to be redesigned.

	va_start(lst,fmt);
	int ret = vswprintf_s(text,fmt,lst);
	text[4095]=0;
	va_end(lst);

	clPrintW(handle, text, ret);

	return ret;
}

void clSetWindowTitle(clHandle console, const wchar_t* windowTitle)
{
	SendMessage(console->windowHandle,WM_SETTEXT, 0, (LPARAM)windowTitle);
}
