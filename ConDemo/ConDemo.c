// ConDemo - ConLib Sample
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
#include "../ConLib/ConLib.h"
#include "targetver.h"
#include <stdio.h>
#include <windows.h>

bool not_closed=true;

int __stdcall notification_callback(ConLibHandle handle, int code, intptr_t wParam, uintptr_t lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    // This is called from the console's update thread so it's NOT safe to do much stuff here!
    // The ideal usage would be to set an event or similar, which is handled from the main thread.
    // For the purposes of this sample, this is more than enough (it is also possible to check clPrint* for a <=0 return value as a failure code)
    if (code == CONSOLE_NOTIFY_CLOSE)
    {
        if (MessageBox((HWND) ConLibGetControlParameter(handle, CONSOLE_SYSTEM_IDENTIFIER),
            L"Are you sure you want to close the console?", L"Question", MB_YESNO) == IDNO)
            return 0;

        not_closed = false;
    }
    return 1;
}

int main()
{
    int i;
    ConLibHandle handle;
	ConLibCreationParameters parameters = {80, 320, 80, 10};

    printf("Initializing ConLib...\n");
	
	parameters.defaultAttribute = CONSOLE_MAKE_ATTRIBUTE(0,24,24,24,0,0,0);
	parameters.preferredCharacterHeight = 15;
	wcscpy(parameters.fontFamily, L"MS Mincho");

    handle = ConLibCreateConsole(&parameters);
    if (handle == NULL)
    {
        printf("Error creating ConLib console!\n");
        return 1;
    }

    ConLibSetWindowTitle(handle, L"ConLib Demo Console");

    ConLibSetNotificationCallback(handle, notification_callback);

    ConLibWPrintf(handle, L"語\r\n");

    for (i=0;i<32;i++)
    {
        ConLibSetControlParameter(handle, CONSOLE_CURRENT_ATTRIBUTE, CONSOLE_MAKE_ATTRIBUTE(i&1,i,31-i,0,31-i,0,i));
        ConLibPrintf(handle, "\r\nHello console!Hello console!Hello console!Hello console!Hello console!!!");
    }

	ConLibSetControlParameter(handle, CONSOLE_CURRENT_ATTRIBUTE, ConLibGetControlParameter(handle, CONSOLE_DEFAULT_ATTRIBUTE));

    ConLibWPrintf(handle, L"\r\nUnicode crap: \r\n暑\r\nい\r\n日\r\n 汉\r\n语\r\n/\r\n漢\r\n語\r\n");
    ConLibWPrintf(handle, L"\r\nUnicode crap (2): |暑い日汉语漢語|\r\n");
    ConLibWPrintf(handle, L"\r\nUnicode crap (2): |..............|\r\n");

    ConLibSetControlParameter(handle, CONSOLE_DEFAULT_ATTRIBUTE, CONSOLE_MAKE_ATTRIBUTE(0,0,0,0,0,31,31));
    ConLibClearLine(handle);

    while (not_closed)
    {
        Sleep(100);
        //ConLibPrintf(handle, "\r\nHello console!Hello console!Hello console!Hello console!Hello console!!!");
    }

    ConLibDestroyConsole(handle);

    return 0;
}

