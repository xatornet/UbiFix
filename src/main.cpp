#include <windows.h>
#include <fstream>
#include <string>
#include <algorithm>
#include "MinHook.h"

// --- REDIRECCIÓN DE REGISTRO (Agresiva) ---
typedef LSTATUS (WINAPI *REGQUERYVALUEEXA)(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
REGQUERYVALUEEXA fpRegQueryValueExA = NULL;

typedef LSTATUS (WINAPI *REGENUMVALUEA)(HKEY, DWORD, LPSTR, LPDWORD, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
REGENUMVALUEA fpRegEnumValueA = NULL;

// Helper para cambiar D: por C: en buffers de memoria
void PatchBuffer(LPBYTE lpData, LPDWORD lpcbData, LPDWORD lpType) {
    if (lpData && lpcbData && *lpcbData > 2 && (*lpType == REG_SZ || *lpType == REG_EXPAND_SZ)) {
        if ((lpData[0] == 'D' || lpData[0] == 'd') && lpData[1] == ':') {
            lpData[0] = 'C';
        }
    }
}

LSTATUS WINAPI DetourRegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    LSTATUS status = fpRegQueryValueExA(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
    if (status == ERROR_SUCCESS) {
        PatchBuffer(lpData, lpcbData, lpType);
    }
    return status;
}

LSTATUS WINAPI DetourRegEnumValueA(HKEY hKey, DWORD dwIndex, LPSTR lpValueName, LPDWORD lpcchValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    LSTATUS status = fpRegEnumValueA(hKey, dwIndex, lpValueName, lpcchValueName, lpReserved, lpType, lpData, lpcbData);
    if (status == ERROR_SUCCESS) {
        PatchBuffer(lpData, lpcbData, lpType);
    }
    return status;
}

// --- REDIRECCIÓN DE ARCHIVOS (Kernel Base) ---
typedef HANDLE (WINAPI *CREATEFILEA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
CREATEFILEA fpCreateFileA = NULL;

HANDLE WINAPI DetourCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    if (lpFileName && (lpFileName[0] == 'C' || lpFileName[0] == 'c') && lpFileName[1] == ':') {
        std::string redirected = lpFileName;
        redirected[0] = 'D'; // Redirigir físicamente a la unidad real
        return fpCreateFileA(redirected.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    }
    return fpCreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

// --- GETMODULEFILENAME (Ubicación del Proceso) ---
typedef DWORD (WINAPI *GETMODULEFILENAMEA)(HMODULE, LPSTR, DWORD);
GETMODULEFILENAMEA fpGetModuleFileNameA = NULL;

DWORD WINAPI DetourGetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize) {
    DWORD result = fpGetModuleFileNameA(hModule, lpFilename, nSize);
    if (result > 0 && lpFilename) {
        if (lpFilename[0] == 'D' || lpFilename[0] == 'd') {
            lpFilename[0] = 'C';
        }
    }
    return result;
}

void Init() {
    if (MH_Initialize() != MH_OK) return;

    // Registro: Engañamos al juego diciéndole que su InstallDir está en C:
    MH_CreateHookApi(L"advapi32", "RegQueryValueExA", (LPVOID)DetourRegQueryValueExA, (LPVOID*)&fpRegQueryValueExA);
    MH_CreateHookApi(L"advapi32", "RegEnumValueA", (LPVOID)DetourRegEnumValueA, (LPVOID*)&fpRegEnumValueA);

    // Archivos: Redirigimos sus peticiones de C: a tu unidad D: real
    MH_CreateHookApi(L"kernel32", "CreateFileA", (LPVOID)DetourCreateFileA, (LPVOID*)&fpCreateFileA);

    // Módulo: Mentimos sobre la ruta del ejecutable
    MH_CreateHookApi(L"kernel32", "GetModuleFileNameA", (LPVOID)DetourGetModuleFileNameA, (LPVOID*)&fpGetModuleFileNameA);

    MH_EnableHook(MH_ALL_HOOKS);
}

BOOL WINAPI DllMain(HMODULE hInst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInst);
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Init, NULL, 0, NULL);
    }
    return TRUE;
}
