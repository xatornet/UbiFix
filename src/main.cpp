#include <windows.h>
#include <fstream>
#include <string>
#include <ctime>
#include "MinHook.h"

// --- SISTEMA DE LOGGING ---
void WriteToLog(const std::string& text) {
    // Abrimos en modo append para no borrar lo anterior
    std::ofstream logFile("path_fix_log.txt", std::ios_base::app);
    if (logFile.is_open()) {
        std::time_t now = std::time(nullptr);
        char timestamp[20];
        struct tm timeinfo;
        localtime_s(&timeinfo, &now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
        
        logFile << "[" << timestamp << "] " << text << std::endl;
        logFile.flush(); // Forzamos la escritura en el disco
        logFile.close();
    }
}

// --- PUNTEROS PARA LAS FUNCIONES ORIGINALES ---
typedef DWORD (WINAPI *GETMODULEFILENAMEA)(HMODULE, LPSTR, DWORD);
typedef DWORD (WINAPI *GETMODULEFILENAMEW)(HMODULE, LPWSTR, DWORD);

GETMODULEFILENAMEA fpGetModuleFileNameA = NULL;
GETMODULEFILENAMEW fpGetModuleFileNameW = NULL;

// --- FUNCIONES INTERCEPTORAS (DETOURS) ---

// Versión ANSI (Normalmente usada por juegos antiguos)
DWORD WINAPI DetourGetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize) {
    DWORD result = fpGetModuleFileNameA(hModule, lpFilename, nSize);
    
    if (result > 0 && lpFilename != NULL) {
        if (lpFilename[0] == 'D' || lpFilename[0] == 'd') {
            std::string original(lpFilename);
            lpFilename[0] = 'C'; // Cambiamos D:\ por C:\ en la respuesta
            WriteToLog("ANSI HOOK: Redirigido " + original + " -> " + std::string(lpFilename));
        }
    }
    return result;
}

// Versión Unicode (Usada por motores más modernos)
DWORD WINAPI DetourGetModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize) {
    DWORD result = fpGetModuleFileNameW(hModule, lpFilename, nSize);
    
    if (result > 0 && lpFilename != NULL) {
        if (lpFilename[0] == L'D' || lpFilename[0] == L'd') {
            std::wstring wOriginal(lpFilename);
            lpFilename[0] = L'C'; // Cambiamos D:\ por C:\ 
            
            // Convertimos a string para el log
            std::string logMsg(wOriginal.begin(), wOriginal.end());
            WriteToLog("WIDE HOOK: Redirigido " + logMsg + " -> C" + logMsg.substr(1));
        }
    }
    return result;
}

// --- INICIALIZACIÓN DE MINHOOK ---
void InitHooks() {
    // 1. Alertamos de que el plugin ha entrado en el proceso
    MessageBoxA(NULL, 
        "Plugin PathFix cargado.\nSi el juego no inicia, revisa path_fix_log.txt", 
        "DEBUG: Ghost Recon Fix", 
        MB_OK | MB_ICONINFORMATION);

    WriteToLog("=== Sesión Iniciada ===");

    if (MH_Initialize() != MH_OK) {
        WriteToLog("Error: No se pudo inicializar MinHook.");
        return;
    }

    // Hookeamos ambas versiones de la API de obtención de ruta
    if (MH_CreateHookApi(L"kernel32", "GetModuleFileNameA", &DetourGetModuleFileNameA, (LPVOID*)&fpGetModuleFileNameA) != MH_OK ||
        MH_CreateHookApi(L"kernel32", "GetModuleFileNameW", &DetourGetModuleFileNameW, (LPVOID*)&fpGetModuleFileNameW) != MH_OK) {
        WriteToLog("Error: Falló la creación de hooks en Kernel32.");
    }

    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        WriteToLog("Error: No se pudieron activar los hooks.");
    } else {
        WriteToLog("Hooks activados con éxito. Interceptando rutas...");
    }
}

// --- ENTRADA DE LA DLL ---
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            InitHooks();
            break;
        case DLL_PROCESS_DETACH:
            MH_Uninitialize();
            break;
    }
    return TRUE;
}
