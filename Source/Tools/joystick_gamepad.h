#ifndef JOYSYICK_GAMEPAD_H_INCLUDED
#define JOYSYICK_GAMEPAD_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>

#include "../Tools/PlatformCompat.h"

#if defined(_WIN32) && SEA_AIR_ENABLE_JOYSTICK
#include <Windows.h>
#include <MMSystem.h>
#pragma comment(lib, "Winmm.lib")
#elif !defined(_WIN32)
typedef struct tagJOYINFOEX
{
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwXpos;
    DWORD dwYpos;
    DWORD dwZpos;
    DWORD dwRpos;
    DWORD dwUpos;
    DWORD dwVpos;
    DWORD dwButtons;
    DWORD dwButtonNumber;
    DWORD dwPOV;
    DWORD dwReserved1;
    DWORD dwReserved2;
} JOYINFOEX, *PJOYINFOEX, *LPJOYINFOEX;

#ifndef JOY_RETURNALL
#define JOY_RETURNALL 0
#endif
#endif

struct JOYINFO_OUTPUT
{
    double dwSize;
    double dwFlags;
    int dwXpos;
    int dwYpos;
    int dwZpos;
    double dwRpos;
    double dwUpos;
    double dwVpos;
    int dwButtons;
    double dwButtonNumber;
    int dwPOV;
    double dwReserved1;
    double dwReserved2;
};

void JoystickInit(JOYINFOEX* joy);

bool getJoystick(JOYINFOEX* pji);

void trans(JOYINFOEX* pji, int type, double range_begin, double range_end);

int Joystick_OutPut(JOYINFOEX* pji, void* data, double dt);

#if !defined(_WIN32) || !SEA_AIR_ENABLE_JOYSTICK
inline void JoystickInit(JOYINFOEX* joy)
{
    if (joy != nullptr) {
        joy->dwSize = sizeof(JOYINFOEX);
        joy->dwFlags = JOY_RETURNALL;
    }
}

inline bool getJoystick(JOYINFOEX*)
{
    return false;
}

inline void trans(JOYINFOEX*, int, double, double)
{
}

inline int Joystick_OutPut(JOYINFOEX*, void*, double)
{
    return -1;
}
#endif

#endif // TOOL_FUNCTION_H_INCLUDED
