// ConLib - Win32 Replacement ConLib Library
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

#include "ConLibCallbacks.h"

#ifdef INTERNAL_DEFINES
#define EXPORTED(ret) extern "C" ret __declspec(dllexport) __stdcall
#else
#define EXPORTED(ret) extern "C" ret __declspec(dllimport) __stdcall
typedef struct conLibPrivateData {}* ConLibHandle;
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Defines

// Limits: these are artificial limits only, they can be modified,
//         but performance and stability are not guaranteed if they are too big
#define CONSOLE_MIN_BUFFER_WIDTH		16
#define CONSOLE_MIN_BUFFER_HEIGHT	1
#define CONSOLE_MAX_BUFFER_WIDTH		2048
#define CONSOLE_MAX_BUFFER_HEIGHT	65536

#define CONSOLE_MIN_WINDOW_WIDTH		CONSOLE_MIN_BUFFER_WIDTH
#define CONSOLE_MIN_WINDOW_HEIGHT	CONSOLE_MIN_BUFFER_HEIGHT
#define CONSOLE_MAX_WINDOW_WIDTH		CONSOLE_MAX_BUFFER_WIDTH
#define CONSOLE_MAX_WINDOW_HEIGHT	2048

// Control parameters
#define CONSOLE_PARAMETER(p_ConLibass,p_id) ((p_ConLibass<<16)|p_id)

// Parameter ConLibasses (pretty useless right now, I know)
#define CONSOLE_CURSOR_CLASS		0x0001
#define CONSOLE_ATTRIBUTE_CLASS	0x0002
#define CONSOLE_SCROLL_CLASS		0x0003

// Parameters
#define CONSOLE_CURSOR_X				CONSOLE_PARAMETER(CONSOLE_CURSOR_CLASS,0x0000)
#define CONSOLE_CURSOR_Y				CONSOLE_PARAMETER(CONSOLE_CURSOR_CLASS,0x0001)

#define CONSOLE_CURRENT_ATTRIBUTE	CONSOLE_PARAMETER(CONSOLE_ATTRIBUTE_CLASS,0x0000)

#define CONSOLE_SCROLL_OFFSET		CONSOLE_PARAMETER(CONSOLE_SCROLL_CLASS,0x0000)

// Macros
#define CONSOLE_MAKE_ATTRIBUTE(bold,fr,fg,fb,br,bg,bb) (((bold)<<31)|((fr)<<26)|((fg)<<21)|((fb)<<16)|((br)<<10)|((bg)<<5)|((bb)<<0))

// Notifications
#define CONSOLE_NOTIFY_CLOSE		0x00000001

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Functions

EXPORTED(ConLibHandle) ConLibCreateConsole(int bufferWidth, int bufferHeight, int windowWidth, int windowHeight);
EXPORTED(void) ConLibDestroyConsole(ConLibHandle ConLib);

EXPORTED(void) ConLibSetWindowTitle(ConLibHandle ConLib, const wchar_t* windowTitle);

EXPORTED(void) ConLibSetControlParameter(ConLibHandle ConLib, int parameterId, int value);
EXPORTED(int)  ConLibGetControlParameter(ConLibHandle ConLib, int parameterId);

EXPORTED(void) ConLibSetNotificationCallback(ConLibHandle ConLib, pConLibNotificationCallback callback);

void inline ConLibGotoXY(ConLibHandle ConLib, int x, int y)
{
	ConLibSetControlParameter(ConLib, CONSOLE_CURSOR_X,x);
	ConLibSetControlParameter(ConLib, CONSOLE_CURSOR_Y,y);
}

// Note: The consoles are wchar_t internally, these functions perform ansi->unicode conversion using
//       the console thread's active code page
EXPORTED(int) ConLibPrint(ConLibHandle handle, char* text, int length);
EXPORTED(int) ConLibPrintf(ConLibHandle handle, const char* fmt, ...);

EXPORTED(int) ConLibPrintW(ConLibHandle handle, wchar_t* text, int length);
EXPORTED(int) ConLibWPrintf(ConLibHandle handle, const wchar_t* fmt, ...);

#endif//_CONLIB_H_