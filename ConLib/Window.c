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
#include <windowsx.h>
#include <tchar.h>
#include <stdarg.h>
#include <stdio.h>
#include "ConLibInternal.h"

MSG __declspec(thread) messageToPost;

bool classRegistered=false;

LONG timerNextID=0;

static void clTimedPostMessage(ConLibHandle handle, int delay, HWND hwnd, int msg, WPARAM wParam, LPARAM lParam)
{
	LONG id = InterlockedExchangeAdd(&timerNextID,1);

	messageToPost.hwnd = hwnd;
	messageToPost.message = msg;
	messageToPost.wParam = wParam;
	messageToPost.lParam = lParam;

	SetTimer(hwnd,handle->idThread,500,NULL);
};

void clUpdateScrollBars(ConLibHandle handle)
{
	HWND hwnd = handle->windowHandle;

	SCROLLINFO si;
	ZeroMemory(&si, sizeof(si));

	if(handle->windowHeight>=handle->bufferHeight)
	{
		handle->scrollOffsetY=0;
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

static void clOnCreate(HWND hwnd, LPCREATESTRUCT lpcs)
{
	SetWindowLongPtr(hwnd, GWL_USERDATA, (LONG_PTR)lpcs->lpCreateParams);
}

static int clIsFullWidthStart(ConLibHandle handle, int x, int y)
{
	charAttribute attr;

	attr.all = handle->attributeRows[y][x];

	return attr.isFullWidthStart;
}

static void clOnStartSelection(ConLibHandle handle, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);

	SetCapture(hwnd);

	handle->selectionMode = 1;
	if(wParam&MK_CONTROL)
		handle->selectionMode = 2;
	else if(wParam&MK_SHIFT)
		handle->selectionMode = 3;

	handle->selectionStartX = (x / handle->characterWidth)+handle->scrollOffsetX; 
	handle->selectionStartY = (y / handle->characterHeight)+handle->scrollOffsetY; 

	if(clIsFullWidthStart(handle, handle->selectionStartX, handle->selectionStartY))
	{
		handle->selectionEndX++;
	}

	handle->selectionEndX = handle->selectionStartX; 
	handle->selectionEndY = handle->selectionStartY; 
	handle->mouseLIsPressed = true;

	InvalidateRect(hwnd,NULL,FALSE);
}

static void clOnContinueSelection(ConLibHandle handle, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);

	KillTimer(hwnd,handle->idThread);

	if((handle->selectionMode>0)&&(handle->mouseLIsPressed))
	{
		handle->selectionEndX = (x / handle->characterWidth)+handle->scrollOffsetX; 
		handle->selectionEndY = (y / handle->characterHeight)+handle->scrollOffsetY; 
		
		if(clIsFullWidthStart(handle, handle->selectionEndX, handle->selectionEndY))
		{
			handle->selectionEndX++;
		}

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
}

static void clOnEndSelection(ConLibHandle handle, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);

	if(handle->selectionMode>0)
	{
		handle->selectionEndX = (x / handle->characterWidth)+handle->scrollOffsetX; 
		handle->selectionEndY = (y / handle->characterHeight)+handle->scrollOffsetY; 

		if(clIsFullWidthStart(handle, handle->selectionEndX, handle->selectionEndY))
		{
			handle->selectionEndX++;
		}

		InvalidateRect(hwnd,NULL,FALSE);
	}
	handle->mouseLIsPressed = false;

	KillTimer(hwnd,handle->idThread);
	ReleaseCapture();
}

static void clOnKeyDown(ConLibHandle handle, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
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
}

static void clOnKeyUp(ConLibHandle handle, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
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
				charAttribute prevAttr;
				prevAttr.all = 0;
				for(x=handle->selectionStartX;x<handle->bufferWidth;x++)
				{
					if(!prevAttr.isFullWidthStart)
						copiedText[copiedTextLength++] = (wchar_t)(handle->characterRows[handle->selectionStartY][x]);
					prevAttr.all = (handle->attributeRows[handle->selectionStartY][x]);
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
					charAttribute prevAttr;
					prevAttr.all = 0;
					for(x=0;x<handle->bufferWidth;x++)
					{
						if(!prevAttr.isFullWidthStart)
							copiedText[copiedTextLength++] = (wchar_t)(handle->characterRows[y][x]);
						prevAttr.all = (handle->attributeRows[y][x]);
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
					charAttribute prevAttr;
					prevAttr.all = 0;
					for(x=handle->selectionStartX;x<=handle->selectionEndX;x++)
					{
						if(!prevAttr.isFullWidthStart)
							copiedText[copiedTextLength++] = (wchar_t)(handle->characterRows[y][x]);
						prevAttr.all = (handle->attributeRows[y][x]);
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

}

static void clOnGetMinMaxInfo(ConLibHandle handle, MINMAXINFO *mmi)
{
	RECT wr,cr;

	int xborder, yborder, maxx, maxy, cxm, cym;

	if(!handle)
		return;

	// make sure scrollbar state is updated
	clUpdateScrollBars(handle);

	if(GetWindowRect(handle->windowHandle,&wr)==0) return;
	if(GetClientRect(handle->windowHandle,&cr)==0) return;

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

static void clOnResizeInProgress(ConLibHandle handle, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	if(handle->lastWindowState != wParam)
		InvalidateRect(hwnd,NULL,TRUE);

	handle->lastWindowState = (int)wParam;

	if(wParam != SIZE_MINIMIZED)
	{
		RECT* r = (RECT*)lParam;

		clProcessResize(handle, r, wParam);
	}
}

static void clOnResizeFinished(ConLibHandle handle, HWND hwnd, WPARAM wParam, LPARAM lParam)
{	
	if( (wParam != SIZE_MINIMIZED) && (handle->lastWindowState != wParam))
	{
		RECT r;

		if(GetWindowRect(hwnd,&r)==0) return;

		clProcessResize(handle, &r, 0);

		clUpdateScrollBars(handle);

		InvalidateRect(hwnd,NULL,TRUE);
		UpdateWindow(hwnd);
	}

	handle->lastWindowState = (int)wParam;
}

static int clOnScrollbarUpdate(HWND hwnd, int cmd, int which, int current, int max)
{
	int sbValue;

	SCROLLINFO si;
	ZeroMemory(&si,sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_POS|SIF_PAGE|SIF_RANGE|SIF_TRACKPOS;

	GetScrollInfo(hwnd, which, &si);
				
	sbValue = si.nPos;
				
	switch(cmd)
	{
	case SB_BOTTOM:
		sbValue = si.nMax;
		break;

	case SB_TOP:
		sbValue = si.nMin;
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
	
	if(sbValue != current)
	{
		if(sbValue > max)
			sbValue = max;

		if(sbValue < 0)
			sbValue = 0;

		SetScrollPos(hwnd,which,sbValue,TRUE);
		InvalidateRect(hwnd,NULL,FALSE);
	}

	return sbValue;
}

static LRESULT clOnHorizontalScroll(ConLibHandle handle, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	handle->scrollOffsetX = clOnScrollbarUpdate(hwnd, LOWORD(wParam), SB_HORZ, handle->scrollOffsetX, handle->bufferWidth - handle->windowWidth);
	return 0;
}

static LRESULT clOnVerticalScroll(ConLibHandle handle, HWND hwnd, WPARAM wParam, LPARAM lParam)
{	
	handle->scrollOffsetY = clOnScrollbarUpdate(hwnd, LOWORD(wParam), SB_VERT, handle->scrollOffsetY, handle->bufferHeight - handle->windowHeight);
	return 0;
}

static LRESULT CALLBACK clWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	ConLibHandle handle = (ConLibHandle)GetWindowLongPtr(hwnd, GWL_USERDATA);

	switch(message)
	{
	case WM_CREATE:
		clOnCreate(hwnd, (LPCREATESTRUCT)lParam);
		break;

	case WM_CLOSE:
		if(handle->notificationCallback) 
			return handle->notificationCallback(CONSOLE_NOTIFY_CLOSE, wParam, lParam);
		break;

	case WM_COPYDATA:
		return clCopyDataHandler(handle, (COPYDATASTRUCT*)lParam);

	case WM_TIMER:
		return clWndProc(messageToPost.hwnd,messageToPost.message,messageToPost.wParam,messageToPost.lParam);
		
	case WM_LBUTTONDOWN:
		clOnStartSelection(handle, hwnd, wParam, lParam);
		break;

	case WM_MOUSEMOVE:		
		clOnContinueSelection(handle, hwnd, wParam, lParam);
		break;

	case WM_LBUTTONUP:
		clOnEndSelection(handle, hwnd, wParam, lParam);
		break;

	case WM_KEYDOWN:
		clOnKeyDown(handle, hwnd, wParam, lParam);
		break;

	case WM_KEYUP:
		clOnKeyUp(handle, hwnd, wParam, lParam);
		break;

	case WM_GETMINMAXINFO:
		clOnGetMinMaxInfo(handle, (MINMAXINFO*)lParam);
		break;

	case WM_SIZING:
		clOnResizeInProgress(handle, hwnd, wParam, lParam);
		break;

	case WM_SIZE:
		clOnResizeFinished(handle, hwnd, wParam, lParam);
		break;

	case WM_HSCROLL:
		return clOnHorizontalScroll(handle, hwnd, wParam, lParam);

	case WM_VSCROLL:
		return clOnVerticalScroll(handle, hwnd, wParam, lParam);
		
	case WM_PAINT:
		clUpdateScrollBars(handle);
		clPaintText(handle, hwnd);
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
		wndclass.hbrBackground = NULL;
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
		L"Consolas"
		);

	ShowWindow(window, SW_NORMAL);

	handle->windowHandle = window;

	hdc = GetDC(window);
	SelectObject(hdc,handle->fontBold);

	GetTextMetrics(hdc, &tm);

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

static void clDestroyWindow(ConLibHandle handle)
{
	if (!handle)
		return;

	if (handle->windowHandle)
	{
		DestroyWindow(handle->windowHandle);
		handle->windowHandle = NULL;
	}

	if(handle->sWndName)
	{
		free(handle->sWndName);
		handle->sWndName = NULL;
	}
}

DWORD clThreadProc(ConLibHandle handle)
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

	clDestroyWindow(handle);

	return 0;
}
