#pragma once

#include "../Tools/PlatformCompat.h"

typedef enum
{
    SC_SERVER,
    SC_CLIENT
} SC_TYPE;

typedef enum
{
    INVALID = -1,
    UI_MIN,
    UI_MAX,
    UI_RESTORE,
    UI_HIDE_SELF,
    UI_HIDE_OTHER,
    UI_HIDEHIDE_OTHER,
    UI_SHOW
} CtrlCmd;

extern "C"
{
#if defined(_WIN32)
    SEA_AIR_IMPORT bool InitWG_S();
    SEA_AIR_IMPORT bool InitWG_C();

    SEA_AIR_IMPORT int  SetWGData(void* data, unsigned int count);
    SEA_AIR_IMPORT int  GetWGData(void* data, unsigned int count);

    SEA_AIR_IMPORT int  SetData(void* data, unsigned int size);
    SEA_AIR_IMPORT int  GetData(void* data, unsigned int size);

    SEA_AIR_IMPORT int  MsgWGPeek();

    SEA_AIR_IMPORT void FreeWG_S();
    SEA_AIR_IMPORT void FreeWG_C();

    SEA_AIR_IMPORT bool SetHook();
    SEA_AIR_IMPORT void UnHook();

    SEA_AIR_IMPORT bool InitHook_S();
    SEA_AIR_IMPORT bool InitHook_C();

    SEA_AIR_IMPORT int  SetHookCtrlCmd(CtrlCmd ctrlCmd, SC_TYPE scType);
    SEA_AIR_IMPORT int  GetHookCtrlCmd(CtrlCmd* ctrlCmd, SC_TYPE scType);
    SEA_AIR_IMPORT int  MsgHookPeek(SC_TYPE scType);
    SEA_AIR_IMPORT int SendHookTxt(const char* txt, int size, SC_TYPE scType);
    SEA_AIR_IMPORT int RecvHookTxt(char* txt, int size, SC_TYPE scType);

    SEA_AIR_IMPORT void FreeHook_S();
    SEA_AIR_IMPORT void FreeHook_C();

    SEA_AIR_IMPORT void Release();

    SEA_AIR_IMPORT DWORD GetProccessPID(const char* processName);

    SEA_AIR_IMPORT bool InitPathPlan_S();
    SEA_AIR_IMPORT bool InitPathPlan_C();
    SEA_AIR_IMPORT int SendPathPlanMessage(const char* txt, int size, const char* tarIP, short tarPort, SC_TYPE scType);
    SEA_AIR_IMPORT int RecvPathPlanMessage(char* txt, int size, SC_TYPE scType);
    SEA_AIR_IMPORT void FreePathPlan_S();
    SEA_AIR_IMPORT void FreePathPlan_C();
#else
    inline bool InitWG_S() { return false; }
    inline bool InitWG_C() { return false; }

    inline int SetWGData(void*, unsigned int) { return 0; }
    inline int GetWGData(void*, unsigned int) { return 0; }

    inline int SetData(void*, unsigned int) { return 0; }
    inline int GetData(void*, unsigned int) { return 0; }

    inline int MsgWGPeek() { return 0; }

    inline void FreeWG_S() {}
    inline void FreeWG_C() {}

    inline bool SetHook() { return false; }
    inline void UnHook() {}

    inline bool InitHook_S() { return false; }
    inline bool InitHook_C() { return false; }

    inline int SetHookCtrlCmd(CtrlCmd, SC_TYPE) { return 0; }
    inline int GetHookCtrlCmd(CtrlCmd*, SC_TYPE) { return 0; }
    inline int MsgHookPeek(SC_TYPE) { return 0; }
    inline int SendHookTxt(const char*, int, SC_TYPE) { return 0; }
    inline int RecvHookTxt(char*, int, SC_TYPE) { return 0; }

    inline void FreeHook_S() {}
    inline void FreeHook_C() {}

    inline void Release() {}

    inline DWORD GetProccessPID(const char*) { return 0; }

    inline bool InitPathPlan_S() { return false; }
    inline bool InitPathPlan_C() { return false; }
    inline int SendPathPlanMessage(const char*, int, const char*, short, SC_TYPE) { return 0; }
    inline int RecvPathPlanMessage(char*, int, SC_TYPE) { return 0; }
    inline void FreePathPlan_S() {}
    inline void FreePathPlan_C() {}
#endif
}
