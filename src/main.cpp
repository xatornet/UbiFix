#include <windows.h>
#include <shlwapi.h>
#include <string>
#include <fstream>
#include "MinHook.h"

// --- PROXY PARA DXGI ---
// Esto permite que nuestra DLL se llame dxgi.dll y el juego no se rompa
HMODULE mHinst = NULL;
HMODULE mHinstDLL = NULL;
extern "C" UINT_PTR mProcs[18] = {0};

extern "C" void CreateDXGIFactory_wrapper();
extern "C" void CreateDXGIFactory1_wrapper();
extern "C" void CreateDXGIFactory2_wrapper();

// --- LOGGING ---
void WriteLog(const std::string& text) {
    std::ofstream log("path_fix.log", std::ios::app);
    if (log.is_open()) {
        log << text << std::endl;
    }
}

// --- HOOKS ---
typedef DWORD (WINAPI *GETMODULEFILENAMEA)(HMODULE, LPSTR, DWORD);
GETMODULEFILENAMEA fpGetModuleFileNameA = NULL;

DWORD WINAPI DetourGetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize) {
    DWORD result = fpGetModuleFileNameA(hModule, lpFilename, nSize);
    if (result > 0 && lpFilename != NULL) {
        if (lpFilename[0] == 'D' || lpFilename[0] == 'd') {
            WriteLog("Hook detectado: " + std::string(lpFilename));
            lpFilename[0] = 'C';
        }
    }
    return result;
}

// --- INICIALIZACIÓN ---
void Init() {
    // Cargar la verdadera dxgi.dll del sistema para que el juego funcione
    char path[MAX_PATH];
    GetSystemDirectoryA(path, MAX_PATH);
    strcat_s(path, "\\dxgi.dll");
    mHinstDLL = LoadLibraryA(path);

    if (MH_Initialize() == MH_OK) {
        MH_CreateHookApi(L"kernel32", "GetModuleFileNameA", &DetourGetModuleFileNameA, (LPVOID*)&fpGetModuleFileNameA);
        MH_EnableHook(MH_ALL_HOOKS);
        WriteLog("Hooks de ruta activados modo Proxy.");
    }
    
    MessageBoxA(NULL, "Ghost Recon Path Fix: Cargado vía Proxy dxgi", "Info", MB_OK);
}

BOOL WINAPI DllMain(HMODULE hInst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        mHinst = hInst;
        DisableThreadLibraryCalls(hInst);
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Init, NULL, 0, NULL);
    }
    return TRUE;
}

// Exportaciones necesarias para que el juego crea que somos la DXGI real
extern "C" __declspec(dllexport) HRESULT WINAPI CreateDXGIFactory(REFIID riid, void** ppFactory) {
    if (!mHinstDLL) mHinstDLL = LoadLibraryA("dxgi.dll");
    typedef HRESULT (WINAPI* pCreateDXGIFactory)(REFIID, void**);
    return ((pCreateDXGIFactory)GetProcAddress(mHinstDLL, "CreateDXGIFactory"))(riid, ppFactory);
}

extern "C" __declspec(dllexport) HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void** ppFactory) {
    if (!mHinstDLL) mHinstDLL = LoadLibraryA("dxgi.dll");
    typedef HRESULT (WINAPI* pCreateDXGIFactory1)(REFIID, void**);
    return ((pCreateDXGIFactory1)GetProcAddress(mHinstDLL, "CreateDXGIFactory1"))(riid, ppFactory);
}
