﻿// ConDemo - ConLib Sample
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
// ConDemo.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "../ConLib/ConLib.h"
#include <windows.h>

bool not_closed=true;

int __stdcall notification_callback(int code, INTPTR wParam, UINTPTR lParam)
{
	// This is called from the console's update thread so it's NOT safe to do much stuff here!
	// The ideal usage woudl be to set an event or similar, which is handled from the main thread.
	// For the purposes of this sample, this is more than enough (it is also possible to check clPrint* for a <=0 return value as a failure code)
	if(code == CL_NOTIFY_CLOSE)	
	{
		not_closed = false;
	}
	return 1;
}

int _tmain(int argc, _TCHAR* argv[])
{
	clHandle handle = clCreateConsole(80, 320, 80, 10);

	clSetWindowTitle(handle, L"ConLib Demo Console");

	clSetNotificationCallback(handle, notification_callback);

	for(int i=0;i<32;i++)
	{
		clSetControlParameter(handle, CL_CURRENT_ATTRIBUTE, CL_MAKE_ATTRIBUTE(i&1,i,31-i,0,31-i,0,i));
		clPrintf(handle, "\r\nHello console!Hello console!Hello console!Hello console!Hello console!!!");
	}

	clWPrintf(handle, L"\r\nUnicode crap: \r\n暑\r\nい\r\n日\r\n 汉\r\n语\r\n/\r\n漢\r\n語\r\n");

	while(not_closed)
	{
		Sleep(100);
	}

	clDestroyConsole(handle);

	return 0;
}

