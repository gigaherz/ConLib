#ifndef _CONLIB_CALLBACKS_H_
#define _CONLIB_CALLBACKS_H_

 // (sizeof(void*) == sizeof(__int64)) does not compile.
#ifdef _WIN64
#define INTPTR __int64
#define UINTPTR unsigned __int64
#else
#define INTPTR int
#define UINTPTR unsigned int
#endif

// Used to receive notifications. Most notably, ConLibosing the window.
typedef int (__stdcall* pConLibNotificationCallback)(int notificationId, INTPTR wParam, UINTPTR lParam);


#endif//_CONLIB_CALLBACKS_H_