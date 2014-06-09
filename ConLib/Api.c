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
#pragma warning(push)
#pragma warning(disable:4820)
#pragma warning(disable:4255)
#pragma warning(disable:4668)
#define _CRT_SECURE_NO_WARNINGS

#include "targetver.h"

#include <windows.h>
#include <tchar.h>
#include <stdarg.h>
#include <stdio.h>
#include <windowsx.h>

#pragma warning(pop)

#include "ConLibInternal.h"

ConLibHandle CALLBACK ConLibCreateConsole(ConLibCreationParameters *params)
{
    int y, x;

    ConLibHandle handle = (ConLibHandle) malloc(sizeof(struct conLibPrivateData_t));

    if (!handle)
    {
        return NULL;
    }

    ZeroMemory(handle, sizeof(struct conLibPrivateData_t));

    handle->creationParameters = *params;

    if (handle->creationParameters.bufferWidth < CONSOLE_MIN_BUFFER_WIDTH)		handle->creationParameters.bufferWidth = CONSOLE_MIN_BUFFER_WIDTH;
    if (handle->creationParameters.bufferWidth > CONSOLE_MAX_BUFFER_WIDTH)		handle->creationParameters.bufferWidth = CONSOLE_MAX_BUFFER_WIDTH;

    if (handle->creationParameters.bufferHeight < CONSOLE_MIN_BUFFER_HEIGHT)	handle->creationParameters.bufferHeight = CONSOLE_MIN_BUFFER_HEIGHT;
    if (handle->creationParameters.bufferHeight > CONSOLE_MAX_BUFFER_HEIGHT)	handle->creationParameters.bufferHeight = CONSOLE_MAX_BUFFER_HEIGHT;

    if (handle->creationParameters.windowWidth < CONSOLE_MIN_WINDOW_WIDTH)		handle->creationParameters.windowWidth = CONSOLE_MIN_WINDOW_WIDTH;
    if (handle->creationParameters.windowWidth > CONSOLE_MAX_WINDOW_WIDTH)		handle->creationParameters.windowWidth = CONSOLE_MAX_WINDOW_WIDTH;

    if (handle->creationParameters.windowHeight < CONSOLE_MIN_WINDOW_HEIGHT)	handle->creationParameters.windowHeight = CONSOLE_MIN_WINDOW_HEIGHT;
    if (handle->creationParameters.windowHeight > CONSOLE_MAX_WINDOW_HEIGHT)	handle->creationParameters.windowHeight = CONSOLE_MAX_WINDOW_HEIGHT;

    if (handle->creationParameters.windowWidth > handle->creationParameters.bufferWidth)
        handle->creationParameters.windowWidth = handle->creationParameters.bufferWidth;
    if (handle->creationParameters.windowHeight > handle->creationParameters.bufferHeight)
        handle->creationParameters.windowHeight = handle->creationParameters.bufferHeight;

    if (handle->creationParameters.tabSize == 0) handle->creationParameters.tabSize = DEFAULT_TAB_SIZE;
    if (handle->creationParameters.tabSize < CONSOLE_MIN_TAB_WIDTH) handle->creationParameters.tabSize = CONSOLE_MIN_TAB_WIDTH;
    if (handle->creationParameters.tabSize > CONSOLE_MAX_TAB_WIDTH) handle->creationParameters.tabSize = CONSOLE_MAX_TAB_WIDTH;

    if (handle->creationParameters.tabMode == 0) handle->creationParameters.tabMode = CONSOLE_TAB_WRITE;

    if (handle->creationParameters.fontFamily[0] == 0)
        wcscpy(handle->creationParameters.fontFamily, L"Lucida Console");

    handle->characterBuffer = (unsigned int*) malloc(sizeof(unsigned int) * (handle->creationParameters.bufferHeight*handle->creationParameters.bufferWidth));
    handle->attributeBuffer = (unsigned int*) malloc(sizeof(unsigned int) * (handle->creationParameters.bufferHeight*handle->creationParameters.bufferWidth));
    handle->characterRows = (unsigned int**) malloc(sizeof(unsigned int*) * (handle->creationParameters.bufferHeight));
    handle->attributeRows = (unsigned int**) malloc(sizeof(unsigned int*) * (handle->creationParameters.bufferHeight));

    //int defAttr = CONSOLE_MAKE_ATTRIBUTE(0,4,31,4,0,4,0);

    for (y = 0; y < handle->creationParameters.bufferHeight; y++)
    {
        unsigned int* cRow = handle->characterRows[y] = handle->characterBuffer + (handle->creationParameters.bufferWidth * y);
        unsigned int* aRow = handle->attributeRows[y] = handle->attributeBuffer + (handle->creationParameters.bufferWidth * y);

        for (x = 0; x < handle->creationParameters.bufferWidth; x++)
        {
            cRow[x] = 0x20;
            aRow[x] = handle->creationParameters.defaultAttribute;
        }
    }

    handle->currentAttribute.all = handle->creationParameters.defaultAttribute;
    handle->scrollOffsetY = 0;
    handle->scrollOffsetX = 0;

    handle->notificationCallback = NULL;
#if USE_APC_CALLBACKS
    handle->callerThread = NULL;
#endif

    handle->lastWindowState = SIZE_RESTORED;

    ConLibGotoXY(handle, 0, 0);

    handle->hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) clThreadProc, handle, 0, &handle->idThread);

    while (!handle->windowCreated)
    {
        DWORD code = 0;

        if (GetExitCodeThread(handle->hThread, &code))
        {
            if (code != STILL_ACTIVE)
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
    if (handle == NULL)
        return;

    SendMessage(handle->windowHandle, WM_CLOSE, 0, 0);

    if (WaitForSingleObject(handle->hThread, 1000) == WAIT_TIMEOUT)
    {
        TerminateThread(handle->hThread, 1);
        DestroyWindow(handle->windowHandle);
    }

    if (handle->characterBuffer) free(handle->characterBuffer);
    if (handle->attributeBuffer) free(handle->attributeBuffer);

    free(handle);
}

bool CALLBACK ConLibSetControlParameter(ConLibHandle handle, int parameterId, int value)
{
    switch (parameterId)
    {
    case CONSOLE_CURSOR_X:
        if (value < 0) return false;
        if (value >= handle->creationParameters.bufferWidth) return false;
        handle->cursorX = value;
        break;
    case CONSOLE_CURSOR_Y:
        if (value < 0) return false;
        if (value >= handle->creationParameters.bufferHeight) return false;
        handle->cursorY = value;
        break;
    case CONSOLE_CURRENT_ATTRIBUTE:
        handle->currentAttribute.all = value;
        break;
    case CONSOLE_DEFAULT_ATTRIBUTE:
        handle->creationParameters.defaultAttribute = value;
        break;
    case CONSOLE_SCROLL_OFFSET_X:
        if (value < 0) return false;
        if (value > (handle->creationParameters.bufferWidth - handle->creationParameters.windowWidth)) return false;
        handle->scrollOffsetX = value;
        break;
    case CONSOLE_SCROLL_OFFSET_Y:
        if (value < 0) return false;
        if (value > (handle->creationParameters.bufferHeight - handle->creationParameters.windowHeight)) return false;
        handle->scrollOffsetY = value;
        break;
    case CONSOLE_TAB_WIDTH:
        if (value < CONSOLE_MIN_TAB_WIDTH) return false;
        if (value > CONSOLE_MAX_TAB_WIDTH) return false;
        handle->creationParameters.tabSize = value;
    case CONSOLE_TAB_MODE:
        switch (value)
        {
        case CONSOLE_TAB_MOVE:
        case CONSOLE_TAB_WRITE:
            handle->creationParameters.tabMode = value;
            break;
        default:
            return false;
        }
        break;
    default:
        return false;
    }

    return true;
}

int  CALLBACK ConLibGetControlParameter(ConLibHandle handle, int parameterId)
{
    switch (parameterId)
    {
    case CONSOLE_SYSTEM_IDENTIFIER:
        return handle->windowHandle;
    case CONSOLE_CURSOR_X:
        return handle->cursorX;
    case CONSOLE_CURSOR_Y:
        return handle->cursorY;
    case CONSOLE_CURRENT_ATTRIBUTE:
        return handle->currentAttribute.all;
    case CONSOLE_DEFAULT_ATTRIBUTE:
        return handle->creationParameters.defaultAttribute;
    case CONSOLE_SCROLL_OFFSET_X:
        return handle->scrollOffsetX;
    case CONSOLE_SCROLL_OFFSET_Y:
        return handle->scrollOffsetY;
    case CONSOLE_BUFFER_SIZE_X:
        return handle->creationParameters.bufferWidth;
    case CONSOLE_BUFFER_SIZE_Y:
        return handle->creationParameters.bufferHeight;
    case CONSOLE_WINDOW_SIZE_X:
        return handle->creationParameters.windowWidth;
    case CONSOLE_WINDOW_SIZE_Y:
        return handle->creationParameters.windowHeight;
    case CONSOLE_TAB_WIDTH:
        return handle->creationParameters.tabSize;
    case CONSOLE_TAB_MODE:
        return handle->creationParameters.tabMode;
    case CONSOLE_FONT_WIDTH:
        return handle->characterWidth;
    case CONSOLE_FONT_HEIGHT:
        return handle->characterHeight;
    }
    return -1;
}

int  CALLBACK ConLibPrintA(ConLibHandle handle, char* text, int length)
{
    COPYDATASTRUCT cd;

    cd.cbData = length;
    cd.lpData = text;
    cd.dwData = CONSOLE_DATA_ANSI;

    return SendMessage(handle->windowHandle, WM_COPYDATA, 0, (LPARAM) &cd);
}

int  CALLBACK ConLibPrintW(ConLibHandle handle, wchar_t* text, int length)
{
    COPYDATASTRUCT cd;

    cd.cbData = length * 2;
    cd.lpData = text;
    cd.dwData = CONSOLE_DATA_UNICODE;

    return SendMessage(handle->windowHandle, WM_COPYDATA, 0, (LPARAM) &cd);
}

int CALLBACK ConLibPrintf(ConLibHandle handle, const char* fmt, ...)
{
    int ret, len;
    va_list lst;
    char *text;

    va_start(lst, fmt);
    len = _vsnprintf(NULL, 0, fmt, lst);
    va_end(lst);

    text = (char*) malloc(sizeof(char) * (len + 1));

    va_start(lst, fmt);
    ret = _vsnprintf(text, len, fmt, lst);
    text[len] = 0;
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

    va_start(lst, fmt);
    len = _vsnwprintf(NULL, 0, fmt, lst);
    va_end(lst);

    text = (wchar_t*) malloc(sizeof(wchar_t) * (len + 1));

    va_start(lst, fmt);
    ret = _vsnwprintf(text, len, fmt, lst);
    text[len] = 0;
    va_end(lst);

    ret = wcslen(text); //

    ConLibPrintW(handle, text, ret);

    return ret;
}

void CALLBACK ConLibSetWindowTitle(ConLibHandle handle, const wchar_t* windowTitle)
{
    SendMessage(handle->windowHandle, WM_SETTEXT, 0, (LPARAM) windowTitle);
}

void CALLBACK ConLibSetNotificationCallback(ConLibHandle handle, pConLibNotificationCallback callback)
{
    handle->notificationCallback = callback;
#if USE_APC_CALLBACKS
    handle->callerThread = GetCurrentThread();
#endif
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

    if (nHandle >= 3)
        return INVALID_HANDLE_VALUE;

    ret = handle->handles[nHandle];
    handle->handles[nHandle] = win32FileHandle;
    PostMessage(handle->windowHandle, CLMSG_NOOP, 0, 0); // wake up the message pump so it notices the change of handle

    return ret;
}
