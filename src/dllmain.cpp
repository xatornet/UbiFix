// GhostReconFS-PathFix - ASI Plugin
// Fixes the bug where Ghost Recon Future Soldier requires installation on C:\
// by intercepting Windows API path calls and redirecting them to the real install path.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <detours.h>
#include <string>
#include <fstream>
#include <sstream>
#include <ctime>
#include <filesystem>
#include <algorithm>
#include <cctype>

// ─────────────────────────────────────────────
//  Configuration & Logging
// ─────────────────────────────────────────────

static bool  g_debugMode   = false;
static std::ofstream g_log;
static std::string   g_realInstallPath;   // e.g. "D:\Games\GhostReconFS"
static std::string   g_fakeRoot = "C:";   // what the game expects

static void Log(const std::string& msg)
{
    if (!g_debugMode || !g_log.is_open()) return;

    std::time_t t = std::time(nullptr);
    char timeBuf[32];
    std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", std::localtime(&t));

    g_log << "[" << timeBuf << "] " << msg << "\n";
    g_log.flush();
}

static std::string LoadIni(const std::string& iniPath)
{
    // Returns "1" / "true" / "yes" value of DebugMode key, or ""
    std::ifstream f(iniPath);
    if (!f.is_open()) return "";

    std::string line;
    while (std::getline(f, line))
    {
        // Strip comments
        auto pos = line.find(';');
        if (pos != std::string::npos) line = line.substr(0, pos);

        // Trim
        auto trim = [](std::string s) {
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c){ return !std::isspace(c); }));
            s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c){ return !std::isspace(c); }).base(), s.end());
            return s;
        };
        line = trim(line);
        if (line.empty()) continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key   = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));

        // Case-insensitive key compare
        std::string keyLow = key;
        std::transform(keyLow.begin(), keyLow.end(), keyLow.begin(), ::tolower);

        if (keyLow == "debugmode") return value;
    }
    return "";
}

// ─────────────────────────────────────────────
//  Path Patching Helpers
// ─────────────────────────────────────────────

// Replaces leading "C:\" (case-insensitive) with the real install drive/path prefix.
// Only patches if the path starts with C:\ AND belongs to our install dir.
static std::string PatchPath(const std::string& original)
{
    if (original.size() < 3) return original;

    // Quick check: does it start with C: ?
    if (!(std::tolower((unsigned char)original[0]) == 'c' && original[1] == ':'))
        return original;

    // Build what the game THINKS the full install path looks like on C:
    // g_realInstallPath might be "D:\Games\GhostReconFS"
    // The game-side fake path would be "C:\Games\GhostReconFS" (same dirs, different drive)
    std::string realDrive = g_realInstallPath.substr(0, 2); // e.g. "D:"
    std::string realDirs  = g_realInstallPath.substr(2);    // e.g. "\Games\GhostReconFS"
    std::string fakePath  = "C:" + realDirs;                // e.g. "C:\Games\GhostReconFS"

    // Case-insensitive prefix match
    std::string origLow = original, fakeLow = fakePath;
    std::transform(origLow.begin(), origLow.end(), origLow.begin(), ::tolower);
    std::transform(fakeLow.begin(), fakeLow.end(), fakeLow.begin(), ::tolower);

    if (origLow.find(fakeLow) == 0)
    {
        std::string patched = g_realInstallPath + original.substr(fakePath.size());
        Log("PatchPath: [" + original + "] -> [" + patched + "]");
        return patched;
    }

    return original;
}

static std::wstring PatchPathW(const std::wstring& original)
{
    // Convert to narrow, patch, convert back
    if (original.empty()) return original;
    int sz = WideCharToMultiByte(CP_UTF8, 0, original.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string narrow(sz, '\0');
    WideCharToMultiByte(CP_UTF8, 0, original.c_str(), -1, narrow.data(), sz, nullptr, nullptr);
    narrow.pop_back(); // remove null terminator added by WideCharToMultiByte

    std::string patched = PatchPath(narrow);
    if (patched == narrow) return original;

    int wsz = MultiByteToWideChar(CP_UTF8, 0, patched.c_str(), -1, nullptr, 0);
    std::wstring wide(wsz, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, patched.c_str(), -1, wide.data(), wsz);
    wide.pop_back();
    return wide;
}

// ─────────────────────────────────────────────
//  Hooked API Functions
// ─────────────────────────────────────────────

// --- CreateFileA ---
static HANDLE (WINAPI *Real_CreateFileA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE) = CreateFileA;
HANDLE WINAPI Hook_CreateFileA(LPCSTR lpFileName, DWORD dwAccess, DWORD dwShare,
    LPSECURITY_ATTRIBUTES lpSA, DWORD dwCreation, DWORD dwFlags, HANDLE hTemplate)
{
    if (lpFileName)
    {
        std::string patched = PatchPath(lpFileName);
        return Real_CreateFileA(patched.c_str(), dwAccess, dwShare, lpSA, dwCreation, dwFlags, hTemplate);
    }
    return Real_CreateFileA(lpFileName, dwAccess, dwShare, lpSA, dwCreation, dwFlags, hTemplate);
}

// --- CreateFileW ---
static HANDLE (WINAPI *Real_CreateFileW)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE) = CreateFileW;
HANDLE WINAPI Hook_CreateFileW(LPCWSTR lpFileName, DWORD dwAccess, DWORD dwShare,
    LPSECURITY_ATTRIBUTES lpSA, DWORD dwCreation, DWORD dwFlags, HANDLE hTemplate)
{
    if (lpFileName)
    {
        std::wstring patched = PatchPathW(lpFileName);
        return Real_CreateFileW(patched.c_str(), dwAccess, dwShare, lpSA, dwCreation, dwFlags, hTemplate);
    }
    return Real_CreateFileW(lpFileName, dwAccess, dwShare, lpSA, dwCreation, dwFlags, hTemplate);
}

// --- GetPrivateProfileStringA (INI reads by the game) ---
static DWORD (WINAPI *Real_GetPrivateProfileStringA)(LPCSTR, LPCSTR, LPCSTR, LPSTR, DWORD, LPCSTR) = GetPrivateProfileStringA;
DWORD WINAPI Hook_GetPrivateProfileStringA(LPCSTR lpApp, LPCSTR lpKey, LPCSTR lpDefault,
    LPSTR lpRetBuf, DWORD nSize, LPCSTR lpFileName)
{
    if (lpFileName)
    {
        std::string patched = PatchPath(lpFileName);
        return Real_GetPrivateProfileStringA(lpApp, lpKey, lpDefault, lpRetBuf, nSize, patched.c_str());
    }
    return Real_GetPrivateProfileStringA(lpApp, lpKey, lpDefault, lpRetBuf, nSize, lpFileName);
}

// --- GetPrivateProfileStringW ---
static DWORD (WINAPI *Real_GetPrivateProfileStringW)(LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, DWORD, LPCWSTR) = GetPrivateProfileStringW;
DWORD WINAPI Hook_GetPrivateProfileStringW(LPCWSTR lpApp, LPCWSTR lpKey, LPCWSTR lpDefault,
    LPWSTR lpRetBuf, DWORD nSize, LPCWSTR lpFileName)
{
    if (lpFileName)
    {
        std::wstring patched = PatchPathW(lpFileName);
        return Real_GetPrivateProfileStringW(lpApp, lpKey, lpDefault, lpRetBuf, nSize, patched.c_str());
    }
    return Real_GetPrivateProfileStringW(lpApp, lpKey, lpDefault, lpRetBuf, nSize, lpFileName);
}

// --- RegOpenKeyExA (registry path checks) ---
static LSTATUS (WINAPI *Real_RegOpenKeyExA)(HKEY, LPCSTR, DWORD, REGSAM, PHKEY) = RegOpenKeyExA;
LSTATUS WINAPI Hook_RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult)
{
    if (lpSubKey)
    {
        std::string patched = PatchPath(lpSubKey);
        if (patched != std::string(lpSubKey))
            Log("RegOpenKeyExA patch: " + std::string(lpSubKey) + " -> " + patched);
        return Real_RegOpenKeyExA(hKey, patched.c_str(), ulOptions, samDesired, phkResult);
    }
    return Real_RegOpenKeyExA(hKey, lpSubKey, ulOptions, samDesired, phkResult);
}

// ─────────────────────────────────────────────
//  Init / Shutdown
// ─────────────────────────────────────────────

static void Init()
{
    // Determine the real install path from the EXE location
    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::filesystem::path p(exePath);
    g_realInstallPath = p.parent_path().string();

    // Remove trailing separator
    while (!g_realInstallPath.empty() && (g_realInstallPath.back() == '\\' || g_realInstallPath.back() == '/'))
        g_realInstallPath.pop_back();

    // Load INI  (same folder as the EXE)
    std::string iniPath = p.parent_path().string() + "\\GhostReconFS-PathFix.ini";

    std::string debugVal = LoadIni(iniPath);
    std::string debugLow = debugVal;
    std::transform(debugLow.begin(), debugLow.end(), debugLow.begin(), ::tolower);
    g_debugMode = (debugLow == "1" || debugLow == "true" || debugLow == "yes");

    if (g_debugMode)
    {
        std::string logPath = p.parent_path().string() + "\\GhostReconFS-PathFix.log";
        g_log.open(logPath, std::ios::out | std::ios::trunc);
        Log("=== GhostReconFS-PathFix loaded ===");
        Log("Real install path : " + g_realInstallPath);
        Log("Debug mode        : ON");
        Log("INI path          : " + iniPath);
    }

    // Only hook if the game is NOT on C:
    char drive[4] = {};
    strncpy_s(drive, g_realInstallPath.c_str(), 2);
    drive[2] = '\0';
    std::string driveLow = drive;
    std::transform(driveLow.begin(), driveLow.end(), driveLow.begin(), ::tolower);

    if (driveLow == "c:")
    {
        Log("Game is on C: - hooks not needed, skipping.");
        return;
    }

    Log("Installing hooks via Detours...");

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    DetourAttach(&(PVOID&)Real_CreateFileA,               Hook_CreateFileA);
    DetourAttach(&(PVOID&)Real_CreateFileW,               Hook_CreateFileW);
    DetourAttach(&(PVOID&)Real_GetPrivateProfileStringA,  Hook_GetPrivateProfileStringA);
    DetourAttach(&(PVOID&)Real_GetPrivateProfileStringW,  Hook_GetPrivateProfileStringW);
    DetourAttach(&(PVOID&)Real_RegOpenKeyExA,             Hook_RegOpenKeyExA);

    LONG result = DetourTransactionCommit();
    Log(std::string("DetourTransactionCommit result: ") + (result == NO_ERROR ? "OK" : "FAILED (" + std::to_string(result) + ")"));
}

static void Shutdown()
{
    Log("Removing hooks...");

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    DetourDetach(&(PVOID&)Real_CreateFileA,               Hook_CreateFileA);
    DetourDetach(&(PVOID&)Real_CreateFileW,               Hook_CreateFileW);
    DetourDetach(&(PVOID&)Real_GetPrivateProfileStringA,  Hook_GetPrivateProfileStringA);
    DetourDetach(&(PVOID&)Real_GetPrivateProfileStringW,  Hook_GetPrivateProfileStringW);
    DetourDetach(&(PVOID&)Real_RegOpenKeyExA,             Hook_RegOpenKeyExA);

    DetourTransactionCommit();
    Log("=== GhostReconFS-PathFix unloaded ===");

    if (g_log.is_open()) g_log.close();
}

// ─────────────────────────────────────────────
//  DLL Entry Point
// ─────────────────────────────────────────────

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID /*lpReserved*/)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            Init();
            break;
        case DLL_PROCESS_DETACH:
            Shutdown();
            break;
    }
    return TRUE;
}
