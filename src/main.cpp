#include <windows.h>
#include <fstream>
#include <string>
#include <vector>
#include "MinHook.h"

// --- CONFIGURACIÓN RÁPIDA ---
// Cambia esto si tu unidad real no es D:
const char TARGET_DRIVE_ REAL = 'D'; 
const char FAKE_DRIVE = 'C';

// --- PROTOTIPOS ---
typedef LSTATUS (WINAPI *REGQUERYVALUEEXA)(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
REGQUERYVALUEEXA fpRegQueryValueExA = NULL;

typedef DWORD (WINAPI *GETMODULEFILENAMEA)(HMODULE, LPSTR, DWORD);
GETMODULEFILENAMEA fpRegGetModuleFileNameA = NULL;

// --- LOGGING SEGURO ---
void QuickLog(const std::string& msg) {
    std::ofstream f("PathFix_Critical.log", std::ios::app);
    f << msg << std::endl;
}

// --- DETOURS OPTIMIZADOS ---

// Solo hookeamos QueryValueExA porque es la que más usa el registro para rutas
LSTATUS WINAPI DetourRegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    LSTATUS status = fpRegQueryValueExA(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
    
    if (status == ERROR_SUCCESS && lpData && lpType && (*lpType == REG_SZ || *lpType == REG_EXPAND_SZ)) {
        // Si el dato empieza por D: (o d:), lo cambiamos a C:
        if ((*lpData == 'D' || *lpData == 'd') && lpData[1] == ':') {
            lpData[0] = FAKE_DRIVE;
        }
    }
    return status;
}

DWORD WINAPI DetourGetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize) {
    DWORD result = fpRegGetModuleFileNameA(hModule, lpFilename, nSize);
    if (result > 0 && lpFilename) {
        if ((lpFilename[0] == 'D' || lpFilename[0] == 'd') && lpFilename[1] == ':') {
            lpFilename[0] = FAKE_DRIVE;
        }
    }
    return result;
}

// --- INICIALIZACIÓN SIN HILOS (Más estable para algunos cargadores) ---
void StartHooking() {
    if (MH_Initialize() != MH_OK) {
        QuickLog("Error: MinHook no pudo iniciar.");
        return;
    }

    // Hookeamos solo lo vital para que no crashee
    MH_CreateHookApi(L"advapi32", "RegQueryValueExA", (LPVOID)DetourRegQueryValueExA, (LPVOID*)&fpRegQueryValueExA);
    MH_CreateHookApi(L"kernel32", "GetModuleFileNameA", (LPVOID)DetourGetModuleFileNameA, (LPVOID*)&fpRegGetModuleFileNameA);

    if (MH_EnableHook(MH_ALL_HOOKS) == MH_OK) {
        QuickLog("Hooks aplicados. Sistema en linea.");
        MessageBoxA(NULL, "PathFix: Inyectado (Modo Registro)", "Ghost Recon", MB_OK | MB_ICONINFORMATION);
    }
}

BOOL WINAPI DllMain(HMODULE hInst, DWORD reason, LPVOID) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hInst);
            // Probamos a ejecutarlo directamente. Si el juego se congela al inicio, 
            // vuelve a envolver StartHooking() en un CreateThread.
            StartHooking();
            break;
        case DLL_PROCESS_DETACH:
            MH_Uninitialize();
            break;
    }
    return TRUE;
}
