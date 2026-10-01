// messagebox_dll.c
#include <windows.h>

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;

    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);

        MessageBoxA(
            NULL,
            "The DLL was loaded successfully.",
            "DLL Message",
            MB_OK | MB_ICONINFORMATION
        );
    }

    return TRUE;
}