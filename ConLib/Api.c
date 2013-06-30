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
// API.cpp : Defines the exported functions for the DLL.
//
#define _CRT_SECURE_NO_WARNINGS

#include "targetver.h"

#include <windows.h>
#include <tchar.h>
#include <stdarg.h>
#include <stdio.h>
#include "ConLibInternal.h"
#include <windowsx.h>

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

	handle->tabSize = DEFAULT_TAB_SIZE;
	handle->tabClearsBuffer = true;

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
