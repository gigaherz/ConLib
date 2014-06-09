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
#ifndef _CONLIBINTERNAL_H_
#define _CONLIBINTERNAL_H_

#define CONLIB_INTERNAL_DEFINES

#include "ConLib.h"

#ifdef __cplusplus 
extern "C" {
#endif

#include "cbool.h"
#include "ConLibCallbacks.h"

// WARNING: Tends to cause font alignment issues
//  (bold width does not match normal width on most fonts)
#define ENABLE_BOLD_SUPPORT 0

// WARNING: Probably does not work.
#define USE_APC_CALLBACKS 0

typedef union charAttribute_t {
    struct {
        unsigned int bgColorB : 5;
        unsigned int bgColorG : 5;
        unsigned int bgColorR : 5;
        unsigned int isContinuation : 1;
        unsigned int fgColorB : 5;
        unsigned int fgColorG : 5;
        unsigned int fgColorR : 5;
        unsigned int bold : 1;
    } bits;
    struct {
        unsigned int bg : 15;
        unsigned int : 1;
        unsigned int fg : 15;
        unsigned int : 1;
    } pieces;
    unsigned int all;
} charAttribute;

typedef struct conLibPrivateData_t
{
    ConLibCreationParameters creationParameters;

    unsigned int* characterBuffer;
    unsigned int* attributeBuffer;

    unsigned int** characterRows;
    unsigned int** attributeRows;

    int cursorX;
    int cursorY;

    charAttribute currentAttribute;

    int characterWidth;
    int characterHeight;

    int scrollOffsetY;
    int scrollOffsetX;

    UINT lastWindowState;

    bool scrollBarVisibleX;
    bool scrollBarVisibleY;

    int selectionMode;		// 0 = not selecting anything, 1= linear, 2= block, 3= rectangle
    int selectionStartX;	// Column
    int selectionStartY;	// Row
    int selectionEndX;		// Column
    int selectionEndY;		// Row

    int shiftIsPressed;
    int ctrlIsPressed;

    int mouseLIsPressed;

    pConLibNotificationCallback notificationCallback;

#if USE_APC_CALLBACKS
    HANDLE callerThread;
#endif

    HFONT fontNormal;
    HFONT fontBold;

    HWND windowHandle;
    TCHAR* sWndName;

    HANDLE handles[2];

    HANDLE hThread;
    DWORD idThread;

    bool windowCreated;

}*ConLibHandle;

#define CONSOLE_DATA_ANSI 0
#define CONSOLE_DATA_UNICODE 1

#define CLMSG_NOOP (WM_USER+0)
#define CLMSG_CLEAR (WM_USER+1)

#define DEFAULT_TAB_SIZE 8
#define FULLWIDTH_NOSPACE_FILLER 0x2060

extern DWORD clThreadProc(ConLibHandle handle);

extern void clUpdateScrollBars(ConLibHandle handle);
extern int clInternalWrite(ConLibHandle handle, const wchar_t* data, int characters);

extern void clHandleWindowResize(ConLibHandle handle, int* width, int* height);
extern void clClearArea(ConLibHandle handle, int mode);

extern void clPaintText(ConLibHandle handle, HWND hwnd);

extern int clIsFullWidthStart(ConLibHandle handle, int x, int y);

extern int clSendCallback(ConLibHandle handle, int code, intptr_t wParam, intptr_t lParam);

void __inline swap(int* a, int* b)
{
    int t = *a;
    *a = *b;
    *b = t;
}

#ifdef __cplusplus 
}
#endif

#endif // _CONLIBINTERNAL_H_
