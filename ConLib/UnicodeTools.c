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
// UnicodeTools.cpp : Tools to work with unicode characters
//
#include "targetver.h"

#include <windows.h>
#include <windowsx.h>
#include "UnicodeTools.h"

#define RANGE(a,b) ((character >= a) && (character <= b))
#define SINGLE(a) (character == a)

bool IsFullWidth1(wchar_t character);

bool isFullWidth[65536];

void setIsFullWidth(LPABCFLOAT floats, float threshold)
{
	int i;
	for(i=0;i<65535;i++)
	{
		if	(floats[i].abcfB > threshold)
			isFullWidth[i] = true;
		else
			isFullWidth[i] = false;
	}
}

bool IsFullWidth(wchar_t character)
{
	return isFullWidth[character] || IsFullWidth1(character);
}

bool IsFullWidth1(wchar_t character)
{
   /* Taken from:
	* EastAsianWidth-6.0.0.txt
	* Date: 2010-08-17, 12:17:00 PDT [KW]
	*
	* East Asian Width Properties
	*
	* This file is an informative contributory data file in the
	* Unicode Character Database.
	*
	* Copyright (c) 1991-2010 Unicode, Inc.
	* For terms of use, see http://www.unicode.org/terms_of_use.html
	*/
#if 0
	// these ranges are "definately no"
	if(RANGE(0x0020,0x007E)) return false;
	if(RANGE(0x00A2,0x00A3)) return false;
	if(RANGE(0x00A5,0x00A6)) return false;
	if(SINGLE(0x00AC)) return false;
	if(SINGLE(0x00AF)) return false;
	if(SINGLE(0x20A9)) return false;
	if(RANGE(0x27E6,0x27ED)) return false;
	if(RANGE(0x2985,0x2986)) return false;
	if(RANGE(0xFF61,0xFFBE)) return false;
	if(RANGE(0xFFC2,0xFFC7)) return false;
	if(RANGE(0xFFCA,0xFFCF)) return false;
	if(RANGE(0xFFD2,0xFFD7)) return false;
	if(RANGE(0xFFDA,0xFFDC)) return false;
	if(RANGE(0xFFE8,0xFFEE)) return false;
#endif
	if(character < 0x8000)
	{
		if(character < 0x3000)
		{
			if(character < 0x2800)
			{
				if(RANGE(0x1100,0x115F)) return true;
				if(RANGE(0x11A3,0x11A7)) return true;
				if(RANGE(0x11FA,0x11FF)) return true;
				if(RANGE(0x2329,0x232A)) return true;
			}
			else
			{
				if(RANGE(0x2E80,0x2E99)) return true;
				if(RANGE(0x2E9B,0x2EF3)) return true;
				if(RANGE(0x2F00,0x2FD5)) return true;
				if(RANGE(0x2FF0,0x2FFB)) return true;
			}
		}
		else
		{
			if(character < 0x3200)
			{
				if(character < 0x3130)
				{
					if(RANGE(0x3000,0x303E)) return true;
					if(RANGE(0x3041,0x3096)) return true;
					if(RANGE(0x3099,0x30FF)) return true;
					if(RANGE(0x3105,0x312D)) return true;
				}
				else
				{
					if(RANGE(0x3131,0x318E)) return true;
					if(RANGE(0x3190,0x31BA)) return true;
					if(RANGE(0x31C0,0x31E3)) return true;
					if(RANGE(0x31F0,0x31FF)) return true;
				}
			}
			else
			{
				if(RANGE(0x3200,0x321E)) return true;
				if(RANGE(0x3220,0x3247)) return true;
				if(RANGE(0x3250,0x32FE)) return true;
				if(RANGE(0x3300,0x33FF)) return true;
				if(RANGE(0x3400,0x4DB5)) return true;
				if(RANGE(0x4DB6,0x4DBF)) return true;
				if(RANGE(0x4E00,0x7FFF)) return true;
			}
		}
	}
	else
	{
		if(character < 0xF000)
		{
			if(character < 0xA800)
			{
				if(RANGE(0x8000,0x9FCB)) return true;
				if(RANGE(0x9FCC,0x9FFF)) return true;
				if(RANGE(0xA000,0xA48C)) return true;
				if(RANGE(0xA490,0xA4C6)) return true;
			}
			else
			{
				if(RANGE(0xA960,0xA97C)) return true;
				if(RANGE(0xAC00,0xD7A3)) return true;
				if(RANGE(0xD7B0,0xD7C6)) return true;
				if(RANGE(0xD7CB,0xD7FB)) return true;
			}
		}
		else
		{
			if(character < 0xFE00)
			{
				if(RANGE(0xF900,0xFA2D)) return true;
				if(RANGE(0xFA2E,0xFA2F)) return true;
				if(RANGE(0xFA30,0xFA6D)) return true;
				if(RANGE(0xFA6E,0xFA6F)) return true;
				if(RANGE(0xFA70,0xFAD9)) return true;
				if(RANGE(0xFADA,0xFAFF)) return true;
			}
			else
			{
				if(RANGE(0xFE10,0xFE19)) return true;
				if(RANGE(0xFE30,0xFE52)) return true;
				if(RANGE(0xFE54,0xFE66)) return true;
				if(RANGE(0xFE68,0xFE6B)) return true;
				if(RANGE(0xFF01,0xFF60)) return true;
				if(RANGE(0xFFE0,0xFFE6)) return true;
			}
		}
	}
#if 0
	// these ranges are on pages > the win32 wchar_t range,
	// no point enabling them since chars > 0xFFFF won't be recognized by my code.
	if(RANGE(0x1B000,0x1B001)) return true;
	if(RANGE(0x1F200,0x1F202)) return true;
	if(RANGE(0x1F210,0x1F23A)) return true;
	if(RANGE(0x1F240,0x1F248)) return true;
	if(RANGE(0x1F250,0x1F251)) return true;
	if(RANGE(0x20000,0x2A6D6)) return true;
	if(RANGE(0x2A6D7,0x2A6FF)) return true;
	if(RANGE(0x2A700,0x2B734)) return true;
	if(RANGE(0x2B735,0x2F73F)) return true;
	if(RANGE(0x2B740,0x2B81D)) return true;
	if(RANGE(0x2B81E,0x2F7FF)) return true;
	if(RANGE(0x2F800,0x2FA1D)) return true;
	if(RANGE(0x2FA1E,0x2FFFD)) return true;
	if(RANGE(0x30000,0x3FFFD)) return true;
#endif
	// default to no
	return false;
}
