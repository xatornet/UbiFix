#include <windows.h>
#include <string>
#include "detours.h"

std::string basePath = "D:\\Instagames\\TC_GR_Future-Soldier\\";

// --- CreateFileA ---
typedef HANDLE (WINAPI* CreateFileA_t)(
    LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES,
    DWORD, DWORD, HANDLE);

CreateFileA_t originalCreateFileA;

HANDLE WINAPI hookedCreateFileA(
    LPCSTR lpFileName,
    DWORD a, DWORD b, LPSECURITY_ATTRIBUTES c,
    DWORD d, DWORD e, HANDLE f)
{
    if (lpFileName && strncmp(lpFileName, "C:\\", 3) == 0) {
        std::string newPath = basePath + (lpFileName + 3);
        return originalCreateFileA(newPath.c_str(), a, b, c, d, e, f);
    }
    return originalCreateFileA(lpFileName, a, b, c, d, e, f);
}

// --- CreateFileW ---
typedef HANDLE (WINAPI* CreateFileW_t)(
    LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES,
    DWORD, DWORD, HANDLE);

CreateFileW_t originalCreateFileW;

HANDLE WINAPI hookedCreateFileW(
    LPCWSTR lpFileName,
    DWORD a, DWORD b, LPSECURITY_ATTRIBUTES c,
    DWORD d, DWORD e, HANDLE f)
{
    if (lpFileName && wcsncmp(lpFileName, L"C:\\", 3) == 0) {
        std::wstring newPath = L"D:\\Instagames\\TC_GR_Future-Soldier\\" + std::wstring(lpFileName + 3);
        return originalCreateFileW(newPath.c_str(), a, b, c, d, e, f);
    }
    return originalCreateFileW(lpFileName, a, b, c, d, e, f);
}

// --- LoadLibraryA ---
typedef HMODULE (WINAPI* LoadLibraryA_t)(LPCSTR);
LoadLibraryA_t originalLoadLibraryA;

HMODULE WINAPI hookedLoadLibraryA(LPCSTR lpLibFileName)
{
    if (lpLibFileName && strncmp(lpLibFileName, "C:\\", 3) == 0) {
        std::string newPath = basePath + (lpLibFileName + 3);
        return originalLoadLibraryA(newPath.c_str());
    }
    return originalLoadLibraryA(lpLibFileName);
}

// --- Hook init ---
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        originalCreateFileA = (CreateFileA_t)GetProcAddress(GetModuleHandleA("kernel32.dll"), "CreateFileA");
        originalCreateFileW = (CreateFileW_t)GetProcAddress(GetModuleHandleA("kernel32.dll"), "CreateFileW");
        originalLoadLibraryA = (LoadLibraryA_t)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");

        DetourAttach(&(PVOID&)originalCreateFileA, hookedCreateFileA);
        DetourAttach(&(PVOID&)originalCreateFileW, hookedCreateFileW);
        DetourAttach(&(PVOID&)originalLoadLibraryA, hookedLoadLibraryA);

        DetourTransactionCommit();
    }
    return TRUE;
}
