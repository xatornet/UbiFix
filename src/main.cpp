#include <windows.h>
#include <fstream>
#include <string>
#include "MinHook.h"

// --- LOGGING ---
void WriteLog(const std::string& text) {
    std::ofstream logFile("C:\\Windows\\Temp\\GhostRecon_PathFix.log", std::ios::app);
    if (logFile.is_open()) {
        logFile << text << std::endl;
        logFile.close();
    }
}

// --- PROTOTIPOS ---
typedef LSTATUS(WINAPI* REGQUERYVALUEEXA)(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
REGQUERYVALUEEXA fpRegQueryValueExA = NULL;
typedef DWORD(WINAPI* GETMODULEFILENAMEA)(HMODULE, LPSTR, DWORD);
GETMODULEFILENAMEA fpGetModuleFileNameA = NULL;

// --- AYUDANTE DE FILTRADO ---
// Solo queremos parchear rutas que pertenezcan al juego
bool IsGamePath(const char* path) {
    if (!path) return false;
    std::string p = path;
    return (p.find("Ghost Recon") != std::string::npos || p.find("Future Soldier") != std::string::npos || p.find("Instagames") != std::string::npos);
}

// --- DETOURS ---

LSTATUS WINAPI DetourRegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    LSTATUS status = fpRegQueryValueExA(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
    
    if (status == ERROR_SUCCESS && lpData && lpType && (*lpType == REG_SZ)) {
        char* data = (char*)lpData;
        if ((data[0] == 'D' || data[0] == 'd') && data[1] == ':' && IsGamePath(data)) {
            data[0] = 'C';
            WriteLog("Registro parcheado con éxito: " + std::string(data));
        }
    }
    return status;
}

DWORD WINAPI DetourGetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize) {
    DWORD result = fpGetModuleFileNameA(hModule, lpFilename, nSize);
    if (result > 0 && lpFilename && (lpFilename[0] == 'D' || lpFilename[0] == 'd')) {
        lpFilename[0] = 'C';
    }
    return result;
}

// --- INICIALIZACIÓN ---
DWORD WINAPI InitializePlugin(LPVOID lpParam) {
    // Un delay un poco más largo para dejar que el sistema de protección del juego se asiente
    Sleep(2000); 

    if (MH_Initialize() == MH_OK) {
        MH_CreateHookApi(L"advapi32", "RegQueryValueExA", (LPVOID)DetourRegQueryValueExA, (LPVOID*)&fpRegQueryValueExA);
        MH_CreateHookApi(L"kernel32", "GetModuleFileNameA", (LPVOID)DetourGetModuleFileNameA, (LPVOID*)&fpGetModuleFileNameA);
        
        if (MH_EnableHook(MH_ALL_HOOKS) == MH_OK) {
            WriteLog("Hooks de precisión aplicados.");
        }
    }
    return 0;
}

BOOL WINAPI DllMain(HMODULE hInst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInst);
        // Creamos el hilo de inicialización
        HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)InitializePlugin, NULL, 0, NULL);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
