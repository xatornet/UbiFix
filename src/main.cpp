#include <windows.h>
#include <fstream>
#include <string>
#include "MinHook.h"

// --- LOGGING DE EMERGENCIA ---
void WriteLog(const std::string& text) {
    // Intentamos escribir en la carpeta temporal del sistema para evitar bloqueos de permisos
    std::ofstream logFile("C:\\Windows\\Temp\\GhostRecon_PathFix.log", std::ios::app);
    if (logFile.is_open()) {
        logFile << text << std::endl;
        logFile.close();
    }
}

// --- PROTOTIPOS ---
typedef LSTATUS(WINAPI* REGQUERYVALUEEXA)(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
typedef LSTATUS(WINAPI* REGQUERYVALUEEXW)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
typedef DWORD(WINAPI* GETMODULEFILENAMEA)(HMODULE, LPSTR, DWORD);
typedef DWORD(WINAPI* GETMODULEFILENAMEW)(HMODULE, LPWSTR, DWORD);

REGQUERYVALUEEXA fpRegQueryValueExA = NULL;
REGQUERYVALUEEXW fpRegQueryValueExW = NULL;
GETMODULEFILENAMEA fpGetModuleFileNameA = NULL;
GETMODULEFILENAMEW fpGetModuleFileNameW = NULL;

// --- DETOURS (ENGAÑO DE UNIDAD) ---

LSTATUS WINAPI DetourRegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    LSTATUS status = fpRegQueryValueExA(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
    if (status == ERROR_SUCCESS && lpData && (*lpType == REG_SZ || *lpType == REG_EXPAND_SZ)) {
        if ((lpData[0] == 'D' || lpData[0] == 'd') && lpData[1] == ':') {
            lpData[0] = 'C';
            WriteLog("Registro ANSI parcheado.");
        }
    }
    return status;
}

LSTATUS WINAPI DetourRegQueryValueExW(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    LSTATUS status = fpRegQueryValueExW(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
    if (status == ERROR_SUCCESS && lpData && (*lpType == REG_SZ || *lpType == REG_EXPAND_SZ)) {
        wchar_t* data = (wchar_t*)lpData;
        if ((data[0] == L'D' || data[0] == L'd') && data[1] == L':') {
            data[0] = L'C';
            WriteLog("Registro Unicode parcheado.");
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

DWORD WINAPI DetourGetModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize) {
    DWORD result = fpGetModuleFileNameW(hModule, lpFilename, nSize);
    if (result > 0 && lpFilename && (lpFilename[0] == L'D' || lpFilename[0] == L'd')) {
        lpFilename[0] = L'C';
    }
    return result;
}

// --- HILO DE INICIALIZACIÓN ---
DWORD WINAPI InitializePlugin(LPVOID lpParam) {
    WriteLog("Iniciando hooks...");
    
    if (MH_Initialize() == MH_OK) {
        // Hookeamos ambas versiones (ANSI y Unicode) para asegurar captura total
        MH_CreateHookApi(L"advapi32", "RegQueryValueExA", (LPVOID)DetourRegQueryValueExA, (LPVOID*)&fpRegQueryValueExA);
        MH_CreateHookApi(L"advapi32", "RegQueryValueExW", (LPVOID)DetourRegQueryValueExW, (LPVOID*)&fpRegQueryValueExW);
        MH_CreateHookApi(L"kernel32", "GetModuleFileNameA", (LPVOID)DetourGetModuleFileNameA, (LPVOID*)&fpGetModuleFileNameA);
        MH_CreateHookApi(L"kernel32", "GetModuleFileNameW", (LPVOID)DetourGetModuleFileNameW, (LPVOID*)&fpGetModuleFileNameW);
        
        MH_EnableHook(MH_ALL_HOOKS);
        WriteLog("Hooks aplicados.");
    }
    return 0;
}

BOOL WINAPI DllMain(HMODULE hInst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInst);
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)InitializePlugin, NULL, 0, NULL);
    }
    return TRUE;
}
