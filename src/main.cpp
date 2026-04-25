#include <windows.h>
#include <string>
#include <vector>
#include "MinHook.h"

// --- PROTOTIPOS ---
typedef LSTATUS(WINAPI* REGQUERYVALUEEXA)(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
REGQUERYVALUEEXA fpRegQueryValueExA = NULL;

typedef DWORD(WINAPI* GETMODULEFILENAMEA)(HMODULE, LPSTR, DWORD);
GETMODULEFILENAMEA fpGetModuleFileNameA = NULL;

// --- DETOURS ULTRALIGEROS (Sin Logs ni Strings pesados) ---

LSTATUS WINAPI DetourRegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    LSTATUS status = fpRegQueryValueExA(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
    
    // Si la función tuvo éxito y el dato es una cadena de texto
    if (status == ERROR_SUCCESS && lpData && lpType && (*lpType == REG_SZ)) {
        // Comprobamos si el dato empieza por D: de forma ultra rápida
        if ((lpData[0] == 'D' || lpData[0] == 'd') && lpData[1] == ':') {
            // Solo parcheamos si detectamos que es una ruta del juego (comprobación básica de longitud)
            // Esto evita tocar claves de sistema irrelevantes
            if (*lpcbData > 10) { 
                lpData[0] = 'C';
            }
        }
    }
    return status;
}

DWORD WINAPI DetourGetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize) {
    DWORD result = fpGetModuleFileNameA(hModule, lpFilename, nSize);
    if (result > 0 && lpFilename) {
        if ((lpFilename[0] == 'D' || lpFilename[0] == 'd') && lpFilename[1] == ':') {
            lpFilename[0] = 'C';
        }
    }
    return result;
}

// --- INICIALIZACIÓN SILENCIOSA ---
void InitializeHooks() {
    if (MH_Initialize() != MH_OK) return;

    MH_CreateHookApi(L"advapi32", "RegQueryValueExA", (LPVOID)DetourRegQueryValueExA, (LPVOID*)&fpRegQueryValueExA);
    MH_CreateHookApi(L"kernel32", "GetModuleFileNameA", (LPVOID)DetourGetModuleFileNameA, (LPVOID*)&fpGetModuleFileNameA);
    
    MH_EnableHook(MH_ALL_HOOKS);
}

BOOL WINAPI DllMain(HMODULE hInst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInst);
        // No creamos hilos nuevos ni usamos Sleep para evitar el congelamiento
        InitializeHooks();
    }
    return TRUE;
}
