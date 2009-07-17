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

#include "stdafx.h"

#include "../ConLib/ConLib.h"
#include <windows.h>

int _tmain(int argc, _TCHAR* argv[])
{
	clHandle handle = clCreateConsole(80, 320, 80, 10);

	clSetWindowTitle(handle, L"ConLib Demo Console");

	for(int i=0;i<32;i++)
	{
		clSetControlParameter(handle, CL_CURRENT_ATTRIBUTE, CL_MAKE_ATTRIBUTE(0,i,31-i,0,31-i,0,i));
		clPrintf(handle, "\r\nHello console!");
	}

	while(clPrintf(handle, "\r")>0)
	{
		Sleep(500);
	}

	clDestroyConsole(handle);

	return 0;
}

