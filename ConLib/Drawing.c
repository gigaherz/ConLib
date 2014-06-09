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
// ConLib.cpp : Defines the exported functions for the DLL application.
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
#include "UnicodeTools.h"


extern MSG __declspec(thread) messageToPost;

static bool clIsContinuation(ConLibHandle handle, int x, int y)
{
    charAttribute attr;

    attr.all = handle->attributeRows[y][x];

    return attr.bits.isContinuation;
}

static void clRepaintText(ConLibHandle handle, HDC hdc, int x, int y, wchar_t* text, int nchars)
{
    TextOut(hdc, x * handle->characterWidth, y*handle->characterHeight, text, nchars);
}

static void clScrollBufferDown(ConLibHandle handle)
{
    int y, x;
    unsigned int* firstCRow = handle->characterRows[0];
    unsigned int* firstARow = handle->attributeRows[0];

    for (y = 1; y < handle->creationParameters.bufferHeight; y++)
    {
        handle->characterRows[y - 1] = handle->characterRows[y];
        handle->attributeRows[y - 1] = handle->attributeRows[y];
    }
    handle->characterRows[handle->creationParameters.bufferHeight - 1] = firstCRow;
    handle->attributeRows[handle->creationParameters.bufferHeight - 1] = firstARow;
    for (x = 0; x < handle->creationParameters.bufferWidth; x++)
    {
        firstCRow[x] = 0x20; // space
        firstARow[x] = handle->creationParameters.defaultAttribute;
    }
}

static void clMoveDown(ConLibHandle handle)
{
    int YScrolled;

    handle->cursorY++;
    if (handle->cursorY >= handle->creationParameters.bufferHeight)
    {
        handle->cursorY--;
        clScrollBufferDown(handle);

        handle->scrollOffsetY = handle->creationParameters.bufferHeight - handle->creationParameters.windowHeight;
        SetScrollPos(handle->windowHandle, SB_VERT, handle->scrollOffsetY, TRUE);

        ScrollWindowEx(handle->windowHandle, 0, -handle->characterHeight, NULL, NULL, NULL, NULL, SW_INVALIDATE);
    }

    YScrolled = handle->cursorY - handle->scrollOffsetY;

    if (YScrolled < 0)
    {
        int scrollDifference = YScrolled;

        if ((-scrollDifference) >= handle->creationParameters.windowHeight)
            InvalidateRect(handle->windowHandle, NULL, FALSE);
        else
            ScrollWindowEx(handle->windowHandle, 0, handle->characterHeight*scrollDifference, NULL, NULL, NULL, NULL, SW_INVALIDATE);

        handle->scrollOffsetY = handle->cursorY;
        //SetScrollPos(handle->windowHandle,SB_VERT,handle->scrollOffsetY,TRUE);
        clUpdateScrollBars(handle);
    }

    if (YScrolled >= handle->creationParameters.windowHeight)
    {
        int scrollDifference = YScrolled - handle->creationParameters.windowHeight + 1;

        if (scrollDifference >= handle->creationParameters.windowHeight)
            InvalidateRect(handle->windowHandle, NULL, FALSE);
        else
            ScrollWindowEx(handle->windowHandle, 0, -handle->characterHeight*scrollDifference, NULL, NULL, NULL, NULL, SW_INVALIDATE);

        handle->scrollOffsetY = handle->cursorY - handle->creationParameters.windowHeight + 1;
        //SetScrollPos(handle->windowHandle,SB_VERT,handle->scrollOffsetY,TRUE);
        clUpdateScrollBars(handle);
    }

    if (handle->scrollOffsetY < 0)
        handle->scrollOffsetY = 0;
}

static void clMoveNext(ConLibHandle handle)
{
    handle->cursorX++;
    if (handle->cursorX >= handle->creationParameters.bufferWidth)
    {
        handle->cursorX = 0;
        clMoveDown(handle);
    }
}

static void clPutCharInBuffer(ConLibHandle handle, wchar_t chr, bool isContinuation)
{
    RECT r;
    charAttribute attr;

    unsigned int* cRow = handle->characterRows[handle->cursorY];
    unsigned int* aRow = handle->attributeRows[handle->cursorY];

    r.left = handle->cursorX * handle->characterWidth;
    r.top = (handle->cursorY - handle->scrollOffsetY)*handle->characterHeight;
    r.right = r.left + handle->characterWidth;
    r.bottom = r.top + handle->characterHeight;

    attr = handle->currentAttribute;

    attr.bits.isContinuation = isContinuation;

    cRow[handle->cursorX] = chr;
    aRow[handle->cursorX] = attr.all;

    InvalidateRect(handle->windowHandle, &r, FALSE);

    clMoveNext(handle);
}

static void clPutChar(ConLibHandle handle, wchar_t chr)
{
    if (chr < 32)
    {
        switch (chr)
        {
        case 10:
            clMoveDown(handle);
            break;
        case 13:
            handle->cursorX = 0;
            break;
        case 9:
            if (handle->creationParameters.tabMode == CONSOLE_TAB_WRITE)
            {
                clPutCharInBuffer(handle, chr, false);
                while ((handle->cursorX % handle->creationParameters.tabSize) != 0)
                    clPutCharInBuffer(handle, chr, true);
            }
            else
            {
                clMoveNext(handle);
                while ((handle->cursorX % handle->creationParameters.tabSize) != 0)
                    clMoveNext(handle);
            }
        }
    }
    else
    {
        clPutCharInBuffer(handle, chr, false);

        if (IsFullWidth(chr)) clPutCharInBuffer(handle, chr, true);
    }
}

int clInternalWrite(ConLibHandle handle, const wchar_t* data, int characters)
{
    int chars;

    for (chars = characters; chars > 0; chars--)
    {
        wchar_t ch = *data++;

        clPutChar(handle, ch);
    }

    //	if(characters>0)
    //		InvalidateRect(handle->windowHandle,NULL, FALSE);

    return characters;
}

void clHandleWindowResize(ConLibHandle handle, int* width, int* height)
{
    int w = *width;
    int h = *height;

    int sw = w / handle->characterWidth;
    int sh = h / handle->characterHeight;

    if (sw < CONSOLE_MIN_WINDOW_WIDTH)	sw = CONSOLE_MIN_WINDOW_WIDTH;
    if (sh < CONSOLE_MIN_WINDOW_HEIGHT)	sh = CONSOLE_MIN_WINDOW_HEIGHT;
    if (sw > CONSOLE_MAX_WINDOW_WIDTH)	sw = CONSOLE_MAX_WINDOW_WIDTH;
    if (sh > CONSOLE_MAX_WINDOW_HEIGHT)	sh = CONSOLE_MAX_WINDOW_HEIGHT;

    if (sw > handle->creationParameters.bufferWidth)	sw = handle->creationParameters.bufferWidth;
    if (sh > handle->creationParameters.bufferHeight)	sh = handle->creationParameters.bufferHeight;

    if (*width == -1)		sw = handle->creationParameters.windowWidth;
    if (*height == -1)	sh = handle->creationParameters.windowHeight;

    *width = sw * handle->characterWidth;
    *height = sh * handle->characterHeight;

    handle->creationParameters.windowWidth = sw;
    handle->creationParameters.windowHeight = sh;

    clSendCallback(handle, CONSOLE_NOTIFY_SIZE_CHANGED, sw, sh);
}

static void clRepaintArea(ConLibHandle handle, int sx, int sy, int ex, int ey)
{
    RECT r;

    r.left = (sx - handle->scrollOffsetX) * handle->characterWidth;
    r.top = (sy - handle->scrollOffsetY) * handle->characterHeight;
    r.right = (ex + 2 - handle->scrollOffsetX) * handle->characterWidth;
    r.bottom = (ey + 2 - handle->scrollOffsetY) * handle->characterHeight;

    InvalidateRect(handle->windowHandle, &r, TRUE);
}

// PRE: x1,x2,y1,y2 need to be in range of the buffer
static void clClearRect(ConLibHandle handle, int x1, int x2, int y1, int y2)
{
    int y, x;
    unsigned int attr = handle->creationParameters.defaultAttribute;
    for (y = y1; y < y2; y++)
    {
        unsigned int* crow = handle->characterRows[y];
        unsigned int* arow = handle->attributeRows[y];
        for (x = x1; x < x2; x++)
        {
            crow[x] = 0x20; // space
            arow[x] = attr;
        }
    }

    clRepaintArea(handle, x1, y1, x2, y2);
}

void clClearArea(ConLibHandle handle, int mode)
{
    int firstLine = 0;
    int lastLine = 0;
    switch (mode)
    {
    case 0: // buffer
        firstLine = 0;
        lastLine = handle->creationParameters.bufferHeight;
        break;
    case 1: // visible
        firstLine = handle->scrollOffsetY;
        lastLine = firstLine + handle->creationParameters.windowHeight;
        break;
    case 2: // current line
        firstLine = handle->cursorY;
        lastLine = firstLine + 1;
        break;
    }

    clClearRect(handle, 0, handle->creationParameters.bufferWidth, firstLine, lastLine);
}

void clPaintText(ConLibHandle handle, HWND hwnd)
{
    COLORREF bColor, fColor;
    int l, r, t, b;
    int selStartI, selEndI, selMode, bufWidth;
    int selStartX, selStartY, selEndX, selEndY;
    bool lastSelected;
    wchar_t* temp;
    int y, x;
    charAttribute lat;

    HDC hdc;
    PAINTSTRUCT paint;

    hdc = BeginPaint(hwnd, &paint);
    {
        if (hdc == NULL)
            return;

        l = paint.rcPaint.left / handle->characterWidth;
        r = (paint.rcPaint.right + handle->characterWidth - 1) / handle->characterWidth;

        t = paint.rcPaint.top / handle->characterHeight;
        b = (paint.rcPaint.bottom + handle->characterHeight - 1) / handle->characterHeight;

        if (r > handle->creationParameters.windowWidth)
            r = handle->creationParameters.windowWidth;
        if (b > handle->creationParameters.windowHeight)
            b = handle->creationParameters.windowHeight;

        lat.all = 0;

        bColor = RGB((lat.bits.bgColorR * 255) / 31, (lat.bits.bgColorG * 255) / 31, (lat.bits.bgColorB * 255) / 31);
        fColor = RGB((lat.bits.fgColorR * 255) / 31, (lat.bits.fgColorG * 255) / 31, (lat.bits.fgColorB * 255) / 31);

        selMode = handle->selectionMode;
        bufWidth = handle->creationParameters.bufferWidth;

        // for sel mode 1
        selStartI = 0;
        selEndI = -1;

        selStartX = handle->selectionStartX;
        selStartY = handle->selectionStartY;
        selEndX = handle->selectionEndX;
        selEndY = handle->selectionEndY;

        if (selEndX < selStartX) swap(&selStartX, &selEndX);
        if (selEndY < selStartY) swap(&selStartY, &selEndY);

        switch (selMode)
        {
        case 1: // standard
            if (selEndY < selStartY || (selEndY == selStartY && selEndX < selStartX))
            {
                swap(&selStartX, &selEndX);
                swap(&selStartY, &selEndY);
            }

            selStartI = (selStartY * bufWidth) + selStartX;
            selEndI = (selEndY * bufWidth) + selEndX;
            break;

        case 2: // block (full lines)
            if (selEndY < selStartY) swap(&selStartY, &selEndY);
            break;

        case 3: // rectangle
            if (selEndX < selStartX) swap(&selStartX, &selEndX);
            if (selEndY < selStartY) swap(&selStartY, &selEndY);
            break;
        }

        lastSelected = false;

        SelectObject(hdc, handle->fontNormal);

        SetTextColor(hdc, fColor);
        SetBkColor(hdc, bColor);

        temp = (wchar_t*) malloc((handle->creationParameters.windowWidth + 1)*sizeof(wchar_t));
        for (y = t; y < b; y++)
        {
            int ay = y + handle->scrollOffsetY;

            unsigned int* cRow = handle->characterRows[ay];
            unsigned int* aRow = handle->attributeRows[ay];
            int nchars = 0;
            int lastx = l;

            int ss = 0, se = 0;

            bool isSelectionRow = (ay >= selStartY) && (ay <= selEndY);

            if (clIsContinuation(handle, l + handle->scrollOffsetX, ay))
            {
                lastx--;
            }

            if (selMode == 1)
            {
                ss = selStartI;
                se = selEndI;

                if (ay == selStartY)
                {
                    if (clIsContinuation(handle, selStartX, selStartY))
                    {
                        ss--;
                    }
                }

                if (ay == selEndY)
                {
                    int st = selEndX + 1;
                    while (st < handle->creationParameters.bufferWidth && clIsContinuation(handle, st, selEndY))
                    {
                        se++;
                        st++;
                    }
                }
            }
            else if (selMode == 3)
            {
                ss = selStartX;
                se = selEndX;

                if (clIsContinuation(handle, ss, ay))
                {
                    ss--;
                }

                while ((se + 1) < handle->creationParameters.bufferWidth && clIsContinuation(handle, se + 1, ay))
                {
                    se++;
                }
            }

            for (x = lastx; x < r; x++)
            {
                int cI;
                bool isSelected = false;
                charAttribute at;

                int ax = x + handle->scrollOffsetX;

                at.all = aRow[ax];

                switch (selMode)
                {
                case 1:
                    cI = (ay * bufWidth) + ax;
                    isSelected = (cI >= ss) && (cI <= se);
                    break;
                case 2:
                    isSelected = isSelectionRow;
                    break;
                case 3:
                    isSelected = isSelectionRow && (ax >= ss) && (ax <= se);
                    break;
                }

                if ((at.pieces.fg != lat.pieces.fg) ||
                    (at.pieces.fg != lat.pieces.fg) ||
#if ENABLE_BOLD_SUPPORT
                    (at.bold != lat.bold) ||
#endif
                    (lastSelected != isSelected))
                {
                    temp[nchars] = 0;
                    if (nchars > 0)
                    {
                        clRepaintText(handle, hdc, lastx, y, temp, nchars);
                    }
                    nchars = 0;
                    lastx = x;

                    if (isSelected)
                    {
                        int t = lat.bits.bgColorB + lat.bits.bgColorG + lat.bits.bgColorR;

                        if (t > 48)
                        {
                            bColor = 0x00000000;
                            fColor = 0x00ffffff;
                        }
                        else
                        {
                            bColor = 0x00ffffff;
                            fColor = 0x00000000;
                        }
                    }
                    else
                    {
                        bColor = RGB((at.bits.bgColorR * 255) / 31, (at.bits.bgColorG * 255) / 31, (at.bits.bgColorB * 255) / 31);
                        fColor = RGB((at.bits.fgColorR * 255) / 31, (at.bits.fgColorG * 255) / 31, (at.bits.fgColorB * 255) / 31);
                    }

#if ENABLE_BOLD_SUPPORT
                    if (at.bold != lat.bold)
                        SelectObject(hdc,(at.bold?handle->fontBold:handle->fontNormal));
#endif

                    SetTextColor(hdc, fColor);
                    SetBkColor(hdc, bColor);
                }

                if (!at.bits.isContinuation)
                    temp[nchars++] = (wchar_t) cRow[ax];

                lat = at;
                lastSelected = isSelected;
            }
            temp[nchars] = 0;
            if (nchars > 0)
            {
                clRepaintText(handle, hdc, lastx, y, temp, nchars);
            }
        }
        free(temp);
    }
    EndPaint(hwnd, &paint);
}
