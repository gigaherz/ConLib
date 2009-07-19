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
#pragma once

#include "ConLibCallbacks.h"

union charAttribute {
	struct {
		int bgColorB : 5;
		int bgColorG : 5;
		int bgColorR : 5;
		int unused1  : 1;
		int fgColorB : 5;
		int fgColorG : 5;
		int fgColorR : 5;
		int bold     : 1;
	};
	int all;
};

typedef struct conLibPrivateData 
{
	int bufferWidth;
	int bufferHeight;

	int* characterBuffer;
	int* attributeBuffer;

	int** characterRows;
	int** attributeRows;

	int windowWidth;
	int windowHeight;

	int cursorX;
	int cursorY;

	charAttribute currentAttribute;
	charAttribute defaultAttribute;

	int characterWidth;
	int characterHeight;

	int scrollOffsetY;
	int scrollOffsetX;

	int lastWindowState;

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

	pclNotificationCallback notificationCallback;

	HFONT fontNormal;
	HFONT fontBold;

	HWND windowHandle;

	HANDLE hThread;
	DWORD idThread;

	bool windowCreated;

}* clHandle;

#define CL_DATA_ANSI 0
#define CL_DATA_UNICODE 1

#define INTERNAL_DEFINES
#include "ConLib.h"
