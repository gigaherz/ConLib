#ifndef _UNICODETOOLS_H_
#define _UNICODETOOLS_H_

#include "cbool.h"

#include <Windows.h>

extern void setIsFullWidth(LPABCFLOAT floats, float threshold);
extern bool IsFullWidth(wchar_t character);

#endif // _UNICODETOOLS_H_
