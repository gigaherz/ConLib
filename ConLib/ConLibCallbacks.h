#ifndef _CONLIB_CALLBACKS_H_
#define _CONLIB_CALLBACKS_H_

#include <stdint.h>

typedef struct conLibPrivateData_t* ConLibHandle;

// Used to receive notifications. Most notably, closing the window.
typedef int(__stdcall* pConLibNotificationCallback)(ConLibHandle handle, int notificationId, intptr_t wParam, uintptr_t lParam);

#endif//_CONLIB_CALLBACKS_H_
