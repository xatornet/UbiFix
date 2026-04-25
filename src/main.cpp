#include <windows.h>
#include <fstream>
#include <string>
#include <ctime>
#include "MinHook.h"

// Función para escribir en el log
void WriteToLog(const std::string& text) {
    std::ofstream logFile("path_fix_log.txt", std::ios_base::app);
    if (logFile.is_open()) {
        std::time_t now = std::time(nullptr);
        char timestamp[20];
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        logFile << "[" << timestamp << "] " << text << std::endl;
    }
}

typedef DWORD (WINAPI *GETMODULEFILENAMEA)(HMODULE, LPSTR, DWORD);
typedef DWORD (WINAPI *GETMODULEFILENAMEW)(HMODULE, LPWSTR, DWORD);

GETMODULEFILENAMEA fpGetModuleFileNameA = NULL;
GETMODULEFILENAMEW fpGetModuleFileNameW = NULL;

DWORD WINAPI DetourGetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize) {
    DWORD result = fpGetModuleFileNameA(hModule, lpFilename, nSize);
    if (result > 0 && lpFilename != NULL) {
        std::string path(lpFilename);
        if (path[0] == 'D' || path[0] == 'd') {
            WriteToLog("Original (A): " + path);
            lpFilename[0] = 'C';
            WriteToLog("Modificado a: " + std::string(lpFilename));
        }
    }
    return result;
}

DWORD WINAPI DetourGetModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize) {
    DWORD result = fpGetModuleFileNameW(hModule, lpFilename, nSize);
    if (result > 0 && lpFilename != NULL) {
        if (lpFilename[0] == L'D' || lpFilename[0] == L'd') {
            // Convertimos brevemente a string para el log
            std::wstring wpath(lpFilename);
            std::string path(wpath.begin(), wpath.end());
            WriteToLog("Original (W): " + path);
            
            lpFilename[0] = L'C';
            
            WriteToLog("Modificado a: C" + path.substr(1));
        }
    }
    return result;
}

void InitHooks() {
    WriteToLog("--- Iniciando Plugin ---");
    if (MH_Initialize() != MH_OK) {
        WriteToLog("Error: No se pudo inicializar MinHook");
        return;
    }

    if (MH_CreateHookApi(L"kernel32", "GetModuleFileNameA", &DetourGetModuleFileNameA, (LPVOID*)&fpGetModuleFileNameA) != MH_OK)
        WriteToLog("Error al crear hook GetModuleFileNameA");
        
    if (MH_CreateHookApi(L"kernel32", "GetModuleFileNameW", &DetourGetModuleFileNameW, (LPVOID*)&fpGetModuleFileNameW) != MH_OK)
        WriteToLog("Error al crear hook GetModuleFileNameW");

    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        WriteToLog("Error: No se pudieron habilitar los hooks");
    } else {
        WriteToLog("Hooks aplicados correctamente");
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        InitHooks();
    }
    return TRUE;
}
