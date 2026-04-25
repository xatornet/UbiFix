#include <windows.h>
#include "MinHook.h"

// Definimos los punteros para guardar las funciones originales
typedef DWORD (WINAPI *GETMODULEFILENAMEA)(HMODULE, LPSTR, DWORD);
typedef DWORD (WINAPI *GETMODULEFILENAMEW)(HMODULE, LPWSTR, DWORD);

GETMODULEFILENAMEA fpGetModuleFileNameA = NULL;
GETMODULEFILENAMEW fpGetModuleFileNameW = NULL;

// Nuestra función falsa (Detour) para la versión ANSI
DWORD WINAPI DetourGetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize) {
    // Llamamos a la función real primero
    DWORD result = fpGetModuleFileNameA(hModule, lpFilename, nSize);
    
    // Si la función tuvo éxito y devolvió una ruta
    if (result > 0 && lpFilename != NULL) {
        // Falsificamos la letra D por la C
        if (lpFilename[0] == 'D' || lpFilename[0] == 'd') {
            lpFilename[0] = 'C';
        }
    }
    return result;
}

// Nuestra función falsa (Detour) para la versión Unicode (Wide)
DWORD WINAPI DetourGetModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize) {
    DWORD result = fpGetModuleFileNameW(hModule, lpFilename, nSize);
    
    if (result > 0 && lpFilename != NULL) {
        if (lpFilename[0] == L'D' || lpFilename[0] == L'd') {
            lpFilename[0] = L'C';
        }
    }
    return result;
}

// Inicializador de los Hooks
void InitHooks() {
    // Inicializar MinHook
    if (MH_Initialize() != MH_OK) return;

    // Crear los hooks para las funciones del Kernel32
    MH_CreateHookApi(L"kernel32", "GetModuleFileNameA", &DetourGetModuleFileNameA, (LPVOID*)&fpGetModuleFileNameA);
    MH_CreateHookApi(L"kernel32", "GetModuleFileNameW", &DetourGetModuleFileNameW, (LPVOID*)&fpGetModuleFileNameW);

    // Activar los hooks
    MH_EnableHook(MH_ALL_HOOKS);
}

// Punto de entrada de la DLL/ASI
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule); // Optimización estándar
            InitHooks();
            break;
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            // Limpieza si el juego se cierra
            if (lpReserved == nullptr) {
                MH_Uninitialize();
            }
            break;
    }
    return TRUE;
}
