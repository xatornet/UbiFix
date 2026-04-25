#include <windows.h>
#include <string>
#include <fstream>
#include <sstream>
#include "detours.h"

std::string basePath = "D:\\Instagames\\TC_GR_Future-Soldier\\";
std::ofstream logFile;

// ---------------- LOG ----------------
void Log(const std::string& msg)
{
    if (!logFile.is_open())
        logFile.open("C:\\temp\\ubifix_log.txt", std::ios::app);

    logFile << msg << std::endl;
    logFile.flush();
}

// ---------------- CreateFileA ----------------
typedef HANDLE (WINAPI* CreateFileA_t)(
    LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES,
    DWORD, DWORD, HANDLE);

CreateFileA_t originalCreateFileA = nullptr;

HANDLE WINAPI hookedCreateFileA(
    LPCSTR lpFileName,
    DWORD a, DWORD b, LPSECURITY_ATTRIBUTES c,
    DWORD d, DWORD e, HANDLE f)
{
    if (lpFileName)
    {
        Log(std::string("[CreateFileA] ") + lpFileName);

        if (strncmp(lpFileName, "C:\\", 3) == 0)
        {
            std::string newPath = basePath + (lpFileName + 3);
            Log(std::string(" -> Redirected to: ") + newPath);
            return originalCreateFileA(newPath.c_str(), a, b, c, d, e, f);
        }
    }
    return originalCreateFileA(lpFileName, a, b, c, d, e, f);
}

// ---------------- CreateFileW ----------------
typedef HANDLE (WINAPI* CreateFileW_t)(
    LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES,
    DWORD, DWORD, HANDLE);

CreateFileW_t originalCreateFileW = nullptr;

HANDLE WINAPI hookedCreateFileW(
    LPCWSTR lpFileName,
    DWORD a, DWORD b, LPSECURITY_ATTRIBUTES c,
    DWORD d, DWORD e, HANDLE f)
{
    if (lpFileName)
    {
        std::wstring ws(lpFileName);
        Log("[CreateFileW] called");

        if (wcsncmp(lpFileName, L"C:\\", 3) == 0)
        {
            std::wstring newPath = L"D:\\Instagames\\TC_GR_Future-Soldier\\" + std::wstring(lpFileName + 3);
            Log("[CreateFileW] redirected");
            return originalCreateFileW(newPath.c_str(), a, b, c, d, e, f);
        }
    }
    return originalCreateFileW(lpFileName, a, b, c, d, e, f);
}

// ---------------- LoadLibraryA ----------------
typedef HMODULE (WINAPI* LoadLibraryA_t)(LPCSTR);
LoadLibraryA_t originalLoadLibraryA = nullptr;

HMODULE WINAPI hookedLoadLibraryA(LPCSTR lpLibFileName)
{
    if (lpLibFileName)
    {
        Log(std::string("[LoadLibraryA] ") + lpLibFileName);

        if (strncmp(lpLibFileName, "C:\\", 3) == 0)
        {
            std::string newPath = basePath + (lpLibFileName + 3);
            Log(std::string(" -> Redirected to: ") + newPath);
            return originalLoadLibraryA(newPath.c_str());
        }
    }
    return originalLoadLibraryA(lpLibFileName);
}

// ---------------- HOOK THREAD ----------------
DWORD WINAPI MainThread(LPVOID)
{
    Log("=== UbiFix START ===");

    Sleep(5000); // 🔥 CRÍTICO: esperar a que el juego cargue

    HMODULE hKernel = GetModuleHandleA("kernel32.dll");

    if (!hKernel)
    {
        Log("ERROR: kernel32.dll not found");
        return 0;
    }

    originalCreateFileA = (CreateFileA_t)GetProcAddress(hKernel, "CreateFileA");
    originalCreateFileW = (CreateFileW_t)GetProcAddress(hKernel, "CreateFileW");
    originalLoadLibraryA = (LoadLibraryA_t)GetProcAddress(hKernel, "LoadLibraryA");

    Log("Got function addresses");

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    DetourAttach(&(PVOID&)originalCreateFileA, hookedCreateFileA);
    DetourAttach(&(PVOID&)originalCreateFileW, hookedCreateFileW);
    DetourAttach(&(PVOID&)originalLoadLibraryA, hookedLoadLibraryA);

    LONG error = DetourTransactionCommit();

    std::stringstream ss;
    ss << "Detour result: " << error;
    Log(ss.str());

    if (error == NO_ERROR)
        Log("Hooks installed OK");
    else
        Log("Hooks FAILED");

    return 0;
}

// ---------------- DLL ENTRY ----------------
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        CreateThread(NULL, 0, MainThread, NULL, 0, NULL);
    }
    return TRUE;
}
