#include <windows.h>
#include <string>
#include "MinHook.h"

// Prototipos originales
typedef LSTATUS(WINAPI* REGQUERYVALUEEXA)(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
REGQUERYVALUEEXA fpRegQueryValueExA = NULL;

typedef DWORD(WINAPI* GETMODULEFILENAMEA)(HMODULE, LPSTR, DWORD);
GETMODULEFILENAMEA fpGetModuleFileNameA = NULL;

// Detour: Engañar sobre el Registro
LSTATUS WINAPI DetourRegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    LSTATUS status = fpRegQueryValueExA(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
    if (status == ERROR_SUCCESS && lpData && lpType && (*lpType == REG_SZ || *lpType == REG_EXPAND_SZ)) {
        if ((lpData[0] == 'D' || lpData[0] == 'd') && lpData[1] == ':') {
            lpData[0] = 'C';
        }
    }
    return status;
}

// Detour: Engañar sobre la ruta del EXE
DWORD WINAPI DetourGetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize) {
    DWORD result = fpGetModuleFileNameA(hModule, lpFilename, nSize);
    if (result > 0 && lpFilename) {
        if ((lpFilename[0] == 'D' || lpFilename[0] == 'd') && lpFilename[1] == ':') {
            lpFilename[0] = 'C';
        }
    }
    return result;
}

// Función de inicialización
DWORD WINAPI InitializePlugin(LPVOID lpParam) {
    // Esperamos un segundo para que el juego termine de cargar sus librerías base
    Sleep(1000);

    if (MH_Initialize() == MH_OK) {
        MH_CreateHookApi(L"advapi32", "RegQueryValueExA", (LPVOID)DetourRegQueryValueExA, (LPVOID*)&fpRegQueryValueExA);
        MH_CreateHookApi(L"kernel32", "GetModuleFileNameA", (LPVOID)DetourGetModuleFileNameA, (LPVOID*)&fpGetModuleFileNameA);
        
        if (MH_EnableHook(MH_ALL_HOOKS) == MH_OK) {
            MessageBoxA(NULL, "PathFix 32-bit: Hooks de Registro Activos", "Ghost Recon", MB_OK);
        }
    }
    return 0;
}

BOOL WINAPI DllMain(HMODULE hInst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInst);
        // Usamos un hilo para no congelar el arranque del juego
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)InitializePlugin, NULL, 0, NULL);
    }
    return TRUE;
}
