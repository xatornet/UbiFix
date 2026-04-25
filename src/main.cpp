#include <windows.h>
#include <fstream>
#include <string>
#include <ctime>
#include "MinHook.h"

bool g_EnableLog = true;
bool g_EnableMessageBox = true;
std::string g_IniPath;

void WriteToLog(const std::string& text) {
    if (!g_EnableLog) return;
    std::ofstream logFile("PathFix.log", std::ios_base::app);
    if (logFile.is_open()) {
        logFile << text << std::endl;
        logFile.close();
    }
}

// --- HOOKS DE UBICACIÓN (GetModuleFileName) ---
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

// --- HOOKS DE ACCESO A ARCHIVOS (CreateFile) ---
typedef HANDLE (WINAPI *CREATEFILEA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef HANDLE (WINAPI *CREATEFILEW)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
CREATEFILEA fpCreateFileA = NULL;
CREATEFILEW fpCreateFileW = NULL;

HANDLE WINAPI DetourCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    std::string path = lpFileName;
    // Si el juego intenta leer en C: (porque le engañamos antes), lo mandamos a D:
    if (path.size() > 3 && (path[0] == 'C' || path[0] == 'c') && path[1] == ':') {
        std::string redirected = path;
        redirected[0] = 'D';
        return fpCreateFileA(redirected.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    }
    return fpCreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

HANDLE WINAPI DetourCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    std::wstring path = lpFileName;
    if (path.size() > 3 && (path[0] == L'C' || path[0] == L'c') && path[1] == L':') {
        std::wstring redirected = path;
        redirected[0] = L'D';
        return fpCreateFileW(redirected.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    }
    return fpCreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

void Init() {
    char asiPath[MAX_PATH];
    GetModuleFileNameA(NULL, asiPath, MAX_PATH);
    std::string strPath = asiPath;
    g_IniPath = strPath.substr(0, strPath.find_last_of("\\/") + 1) + "PathFix.ini";

    g_EnableLog = GetPrivateProfileIntA("Config", "EnableLog", 1, g_IniPath.c_str()) != 0;
    g_EnableMessageBox = GetPrivateProfileIntA("Config", "EnableMessageBox", 1, g_IniPath.c_str()) != 0;

    if (g_EnableMessageBox) MessageBoxA(NULL, "PathFix: Engaño de unidad + Redirección de archivos activo", "Ghost Recon Fix", MB_OK);

    if (MH_Initialize() == MH_OK) {
        // Engañamos con la posición
        MH_CreateHookApi(L"kernel32", "GetModuleFileNameA", (LPVOID)DetourGetModuleFileNameA, (LPVOID*)&fpGetModuleFileNameA);
        // Redirigimos la lectura de archivos de C: a D:
        MH_CreateHookApi(L"kernel32", "CreateFileA", (LPVOID)DetourCreateFileA, (LPVOID*)&fpCreateFileA);
        MH_CreateHookApi(L"kernel32", "CreateFileW", (LPVOID)DetourCreateFileW, (LPVOID*)&fpCreateFileW);
        
        MH_EnableHook(MH_ALL_HOOKS);
        WriteToLog("Hooks de engaño y redirección aplicados.");
    }
}

BOOL WINAPI DllMain(HMODULE hInst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInst);
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Init, NULL, 0, NULL);
    }
    return TRUE;
}
