#include <windows.h>
#include <fstream>
#include <string>
#include <ctime>
#include "MinHook.h"

// Variables de configuración
bool g_EnableLog = true;
bool g_EnableMessageBox = true;
std::string g_IniPath;

// --- SISTEMA DE LOGGING ---
void WriteToLog(const std::string& text) {
    if (!g_EnableLog) return;

    std::ofstream logFile("PathFix.log", std::ios_base::app);
    if (logFile.is_open()) {
        std::time_t now = std::time(nullptr);
        char timestamp[20];
        struct tm timeinfo;
        localtime_s(&timeinfo, &now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
        
        logFile << "[" << timestamp << "] " << text << std::endl;
        logFile.close();
    }
}

// --- HOOKS ---
typedef DWORD (WINAPI *GETMODULEFILENAMEA)(HMODULE, LPSTR, DWORD);
GETMODULEFILENAMEA fpGetModuleFileNameA = NULL;

DWORD WINAPI DetourGetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize) {
    DWORD result = fpGetModuleFileNameA(hModule, lpFilename, nSize);
    
    if (result > 0 && lpFilename != NULL) {
        if (lpFilename[0] == 'D' || lpFilename[0] == 'd') {
            std::string original(lpFilename);
            lpFilename[0] = 'C';
            WriteToLog("Redirigido: " + original + " -> " + std::string(lpFilename));
        }
    }
    return result;
}

// --- INICIALIZACIÓN ---
void Init() {
    // Obtener ruta del INI (en la misma carpeta que el ASI)
    char asiPath[MAX_PATH];
    GetModuleFileNameA(NULL, asiPath, MAX_PATH);
    std::string strPath = asiPath;
    size_t lastSlash = strPath.find_last_of("\\/");
    g_IniPath = strPath.substr(0, lastSlash + 1) + "PathFix.ini";

    // Leer configuración del INI
    // GetPrivateProfileIntA(Sección, Clave, ValorPorDefecto, RutaArchivo)
    g_EnableLog = GetPrivateProfileIntA("Config", "EnableLog", 1, g_IniPath.c_str()) != 0;
    g_EnableMessageBox = GetPrivateProfileIntA("Config", "EnableMessageBox", 1, g_IniPath.c_str()) != 0;

    if (g_EnableMessageBox) {
        MessageBoxA(NULL, "PathFix 32-bit cargado.\nLog y Mensajes configurables en PathFix.ini", "Ghost Recon Fix", MB_OK | MB_ICONINFORMATION);
    }

    WriteToLog("=== Sesión Iniciada (Arquitectura x86) ===");

    if (MH_Initialize() == MH_OK) {
        if (MH_CreateHookApi(L"kernel32", "GetModuleFileNameA", (LPVOID)DetourGetModuleFileNameA, (LPVOID*)&fpGetModuleFileNameA) == MH_OK) {
            MH_EnableHook(MH_ALL_HOOKS);
            WriteToLog("Hooks aplicados correctamente.");
        } else {
            WriteToLog("Error: No se pudo crear el hook para GetModuleFileNameA.");
        }
    } else {
        WriteToLog("Error: No se pudo inicializar MinHook.");
    }
}

BOOL WINAPI DllMain(HMODULE hInst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInst);
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Init, NULL, 0, NULL);
    }
    return TRUE;
}
