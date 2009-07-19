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

#include "ConLibCallbacks.h"

#ifdef INTERNAL_DEFINES
#define EXPORTED(ret) extern "C" ret __declspec(dllexport) __stdcall
#else
#define EXPORTED(ret) extern "C" ret __declspec(dllimport) __stdcall
typedef struct conLibPrivateData {}* clHandle;
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Defines

// Limits: these are artificial limits only, they can be modified,
//         but performance and stability are not guaranteed if they are too big
#define CL_MIN_BUFFER_WIDTH		16
#define CL_MIN_BUFFER_HEIGHT	1
#define CL_MAX_BUFFER_WIDTH		2048
#define CL_MAX_BUFFER_HEIGHT	65536

#define CL_MIN_WINDOW_WIDTH		CL_MIN_BUFFER_WIDTH
#define CL_MIN_WINDOW_HEIGHT	CL_MIN_BUFFER_HEIGHT
#define CL_MAX_WINDOW_WIDTH		CL_MAX_BUFFER_WIDTH
#define CL_MAX_WINDOW_HEIGHT	2048

// Control parameters
#define CL_PARAMETER(p_class,p_id) ((p_class<<16)|p_id)

// Parameter classes (pretty useless right now, I know)
#define CL_CURSOR_CLASS		0x0001
#define CL_ATTRIBUTE_CLASS	0x0002
#define CL_SCROLL_CLASS		0x0003

// Parameters
#define CL_CURSOR_X				CL_PARAMETER(CL_CURSOR_CLASS,0x0000)
#define CL_CURSOR_Y				CL_PARAMETER(CL_CURSOR_CLASS,0x0001)

#define CL_CURRENT_ATTRIBUTE	CL_PARAMETER(CL_ATTRIBUTE_CLASS,0x0000)

#define CL_SCROLL_OFFSET		CL_PARAMETER(CL_SCROLL_CLASS,0x0000)

// Macros
#define CL_MAKE_ATTRIBUTE(bold,fr,fg,fb,br,bg,bb) (((bold)<<31)|((fr)<<26)|((fg)<<21)|((fb)<<16)|((br)<<10)|((bg)<<5)|((bb)<<0))

// Notifications
#define CL_NOTIFY_CLOSE		0x00000001

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Functions

EXPORTED(clHandle) clCreateConsole(int bufferWidth, int bufferHeight, int windowWidth, int windowHeight);
EXPORTED(void) clDestroyConsole(clHandle console);

EXPORTED(void) clSetWindowTitle(clHandle console, const wchar_t* windowTitle);

EXPORTED(void) clSetControlParameter(clHandle console, int parameterId, int value);
EXPORTED(int)  clGetControlParameter(clHandle console, int parameterId);

EXPORTED(void) clSetNotificationCallback(clHandle console, pclNotificationCallback callback);

void inline clGotoXY(clHandle console, int x, int y)
{
	clSetControlParameter(console, CL_CURSOR_X,x);
	clSetControlParameter(console, CL_CURSOR_Y,y);
}

// Note: The consoles are wchar_t internally, these functions perform ansi->unicode conversion using
//       the console thread's active code page
EXPORTED(int) clPrint(clHandle console, char* text, int length);
EXPORTED(int) clPrintf(clHandle console, const char* fmt, ...);

EXPORTED(int) clPrintW(clHandle console, wchar_t* text, int length);
EXPORTED(int) clWPrintf(clHandle console, const wchar_t* fmt, ...);

#endif//_CONLIB_H_