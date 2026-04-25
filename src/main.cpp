#include <windows.h>
#include <fstream>
#include <string>
#include "MinHook.h"

// --- REDIRECCIÓN DE REGISTRO ---
typedef LSTATUS (WINAPI *REGQUERYVALUEEXA)(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
REGQUERYVALUEEXA fpRegQueryValueExA = NULL;

LSTATUS WINAPI DetourRegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    LSTATUS status = fpRegQueryValueExA(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
    
    // Si el juego pide una ruta y contiene "D:", la cambiamos por "C:"
    if (status == ERROR_SUCCESS && lpData != NULL && *lpType == REG_SZ) {
        std::string value = (char*)lpData;
        if (value.size() > 2 && (value[0] == 'D' || value[0] == 'd') && value[1] == ':') {
            ((char*)lpData)[0] = 'C';
        }
    }
    return status;
}

// --- REDIRECCIÓN DE ARCHIVOS ---
typedef HANDLE (WINAPI *CREATEFILEA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
CREATEFILEA fpCreateFileA = NULL;

HANDLE WINAPI DetourCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    std::string path = lpFileName;
    // Si intenta leer en C:, lo mandamos a D: donde están los archivos reales
    if (path.size() > 2 && (path[0] == 'C' || path[0] == 'c') && path[1] == ':') {
        std::string redirected = path;
        redirected[0] = 'D';
        return fpCreateFileA(redirected.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    }
    return fpCreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

// --- REDIRECCIÓN DE UBICACIÓN DEL MÓDULO ---
typedef DWORD (WINAPI *GETMODULEFILENAMEA)(HMODULE, LPSTR, DWORD);
GETMODULEFILENAMEA fpGetModuleFileNameA = NULL;

DWORD WINAPI DetourGetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize) {
    DWORD result = fpGetModuleFileNameA(hModule, lpFilename, nSize);
    if (result > 0 && lpFilename != NULL) {
        if (lpFilename[0] == 'D' || lpFilename[0] == 'd') {
            lpFilename[0] = 'C';
        }
    }
    return result;
}

void Init() {
    if (MH_Initialize() != MH_OK) return;

    // Hook 1: Engañar sobre dónde está el .exe
    MH_CreateHookApi(L"kernel32", "GetModuleFileNameA", (LPVOID)DetourGetModuleFileNameA, (LPVOID*)&fpGetModuleFileNameA);
    
    // Hook 2: Engañar sobre lo que dice el Registro (InstallDir)
    MH_CreateHookApi(L"advapi32", "RegQueryValueExA", (LPVOID)DetourRegQueryValueExA, (LPVOID*)&fpRegQueryValueExA);
    
    // Hook 3: Redirigir la carga de archivos de C: a D:
    MH_CreateHookApi(L"kernel32", "CreateFileA", (LPVOID)DetourCreateFileA, (LPVOID*)&fpCreateFileA);

    MH_EnableHook(MH_ALL_HOOKS);
}

BOOL WINAPI DllMain(HMODULE hInst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInst);
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Init, NULL, 0, NULL);
    }
    return TRUE;
}
