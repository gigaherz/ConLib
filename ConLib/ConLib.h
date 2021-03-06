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
#ifndef _CONLIB_H_
#define _CONLIB_H_

#ifdef __cplusplus 
extern "C" {
#endif

#include "cbool.h"
#include "ConLibCallbacks.h"

#ifdef CONLIB_INTERNAL_DEFINES
#define EXPORTED(ret) ret __declspec(dllexport) __stdcall
#else
#define EXPORTED(ret) ret __declspec(dllimport) __stdcall
#endif

typedef struct conLibPrivateData_t* ConLibHandle;

typedef void* pvoid;

#define inline __inline

typedef struct conLibCreationParameters_t
{
    int bufferWidth;
    int bufferHeight;

    int windowWidth;
    int windowHeight;

    unsigned int defaultAttribute;

    int preferredCharacterWidth;
    int preferredCharacterHeight;

    int tabSize;
    int tabMode;

    wchar_t fontFamily[256];

} ConLibCreationParameters;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Defines

// Limits: these are artificial limits only, they can be modified,
//         but performance and stability are not guaranteed if they are outside
//         of the sensible range.
#define CONSOLE_MIN_BUFFER_WIDTH	16
#define CONSOLE_MIN_BUFFER_HEIGHT	1
#define CONSOLE_MAX_BUFFER_WIDTH	2048
#define CONSOLE_MAX_BUFFER_HEIGHT	65536

#define CONSOLE_MIN_TAB_WIDTH		1
#define CONSOLE_MAX_TAB_WIDTH		31

#define CONSOLE_MIN_WINDOW_WIDTH	CONSOLE_MIN_BUFFER_WIDTH
#define CONSOLE_MIN_WINDOW_HEIGHT	CONSOLE_MIN_BUFFER_HEIGHT
#define CONSOLE_MAX_WINDOW_WIDTH	CONSOLE_MAX_BUFFER_WIDTH
#define CONSOLE_MAX_WINDOW_HEIGHT	2048

#define CONSOLE_MIN_FONT_WIDTH		4
#define CONSOLE_MAX_FONT_WIDTH		255

#define CONSOLE_MIN_FONT_HEIGHT		5
#define CONSOLE_MAX_FONT_HEIGHT		255

#define CONSOLE_TAB_MOVE 1
#define CONSOLE_TAB_WRITE 2

// Control parameters
#define CONSOLE_PARAMETER(p_class,p_id) ((p_class<<16)|p_id)

// Parameter classes (pretty useless right now, I know)
#define CONSOLE_CURSOR_CLASS		0x0001
#define CONSOLE_BUFFER_CLASS		0x0002
#define CONSOLE_WINDOW_CLASS		0x0003

// Parameters
#define CONSOLE_CURSOR_X			CONSOLE_PARAMETER(CONSOLE_CURSOR_CLASS,0x0000)
#define CONSOLE_CURSOR_Y			CONSOLE_PARAMETER(CONSOLE_CURSOR_CLASS,0x0001)

#define CONSOLE_CURRENT_ATTRIBUTE	CONSOLE_PARAMETER(CONSOLE_BUFFER_CLASS,0x0000)
#define CONSOLE_DEFAULT_ATTRIBUTE	CONSOLE_PARAMETER(CONSOLE_BUFFER_CLASS,0x0001)

// TODO: make writable
#define CONSOLE_BUFFER_SIZE_X		CONSOLE_PARAMETER(CONSOLE_BUFFER_CLASS,0x0002)
#define CONSOLE_BUFFER_SIZE_Y		CONSOLE_PARAMETER(CONSOLE_BUFFER_CLASS,0x0003)

#define CONSOLE_TAB_WIDTH			CONSOLE_PARAMETER(CONSOLE_BUFFER_CLASS,0x0004)
#define CONSOLE_TAB_MODE			CONSOLE_PARAMETER(CONSOLE_BUFFER_CLASS,0x0005)

#define CONSOLE_SCROLL_OFFSET_X		CONSOLE_PARAMETER(CONSOLE_WINDOW_CLASS,0x0000)
#define CONSOLE_SCROLL_OFFSET_Y		CONSOLE_PARAMETER(CONSOLE_WINDOW_CLASS,0x0001)

// TODO: make writable
#define CONSOLE_WINDOW_SIZE_X		CONSOLE_PARAMETER(CONSOLE_WINDOW_CLASS,0x0002)
#define CONSOLE_WINDOW_SIZE_Y		CONSOLE_PARAMETER(CONSOLE_WINDOW_CLASS,0x0003)

// TODO: make writable
#define CONSOLE_FONT_WIDTH			CONSOLE_PARAMETER(CONSOLE_WINDOW_CLASS,0x0004)
#define CONSOLE_FONT_HEIGHT			CONSOLE_PARAMETER(CONSOLE_WINDOW_CLASS,0x0005)

#define CONSOLE_SYSTEM_IDENTIFIER	CONSOLE_PARAMETER(CONSOLE_WINDOW_CLASS,0x0006)

// Macros
#define CONSOLE_MAKE_ATTRIBUTE(bold,fr,fg,fb,br,bg,bb) (((bold)<<31)|((fr)<<26)|((fg)<<21)|((fb)<<16)|((br)<<10)|((bg)<<5)|((bb)<<0))

// Notifications
#define CONSOLE_NOTIFY_CLOSE		0x00000001
#define CONSOLE_NOTIFY_SIZE_CHANGED	0x00000002

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Functions

EXPORTED(ConLibHandle) ConLibCreateConsole(ConLibCreationParameters* parameters);
EXPORTED(void) ConLibDestroyConsole(ConLibHandle handle);

EXPORTED(void) ConLibSetWindowTitle(ConLibHandle handle, const wchar_t* windowTitle);

EXPORTED(bool) ConLibSetControlParameter(ConLibHandle handle, int parameterId, int value);
EXPORTED(int)  ConLibGetControlParameter(ConLibHandle handle, int parameterId);

EXPORTED(void) ConLibSetNotificationCallback(ConLibHandle handle, pConLibNotificationCallback callback);

void inline ConLibGotoXY(ConLibHandle handle, int x, int y)
{
	ConLibSetControlParameter(handle, CONSOLE_CURSOR_X, x);
	ConLibSetControlParameter(handle, CONSOLE_CURSOR_Y, y);
}

EXPORTED(void) ConLibClearBuffer(ConLibHandle handle);
EXPORTED(void) ConLibClearVisibleArea(ConLibHandle handle);
EXPORTED(void) ConLibClearLine(ConLibHandle handle);

// Note: The consoles are wchar_t internally, these functions perform ansi->unicode conversion using
//       the console thread's active code page
EXPORTED(int) ConLibPrint(ConLibHandle handle, char* text, int length);
EXPORTED(int) ConLibPrintf(ConLibHandle handle, const char* fmt, ...);

EXPORTED(int) ConLibPrintW(ConLibHandle handle, wchar_t* text, int length);
EXPORTED(int) ConLibWPrintf(ConLibHandle handle, const wchar_t* fmt, ...);

// same as std file numbers, 0=input, 1=output, 2=error/extra
EXPORTED(pvoid) ConLibSetIOHandle(ConLibHandle handle, int nHandle, pvoid win32FileHandle);

#ifdef __cplusplus 
}
#endif

#endif//_CONLIB_H_
