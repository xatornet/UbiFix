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

static bool  g_debugMode   = false;  // DebugMode=1  → log only patched paths
static bool  g_traceMode   = false;  // TraceMode=1  → log EVERY path call (diagnostic)
static std::ofstream g_log;
static std::string   g_realInstallPath;   // e.g. "D:\Games\GhostReconFS"
static std::string   g_hardcodedC  = "";  // HardcodedPath=C:\Program Files (x86)\Ubisoft\...

static void Log(const std::string& msg)
{
    if (!g_log.is_open()) return;
    std::time_t t = std::time(nullptr);
    char timeBuf[32];
    std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", std::localtime(&t));
    g_log << "[" << timeBuf << "] " << msg << "\n";
    g_log.flush();
}

static void LogTrace(const std::string& api, const std::string& path)
{
    if (!g_traceMode || !g_log.is_open()) return;
    Log("TRACE " + api + " [" + path + "]");
}

static std::string LoadIniKey(const std::string& iniPath, const std::string& wantedKey)
{
    std::ifstream f(iniPath);
    if (!f.is_open()) return "";

    auto trim = [](std::string s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c){ return !std::isspace(c); }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c){ return !std::isspace(c); }).base(), s.end());
        return s;
    };

    std::string line;
    while (std::getline(f, line))
    {
        auto pos = line.find(';');
        if (pos != std::string::npos) line = line.substr(0, pos);
        line = trim(line);
        if (line.empty()) continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key   = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        std::string keyLow = key;
        std::transform(keyLow.begin(), keyLow.end(), keyLow.begin(), ::tolower);
        if (keyLow == wantedKey) return value;
    }
    return "";
}

static bool IsTruthy(const std::string& val)
{
    std::string v = val;
    std::transform(v.begin(), v.end(), v.begin(), ::tolower);
    return (v == "1" || v == "true" || v == "yes");
}

// ─────────────────────────────────────────────
//  Path Patching Helpers
// ─────────────────────────────────────────────

static std::string PatchPath(const std::string& original)
{
    if (original.size() < 3) return original;
    if (!(std::tolower((unsigned char)original[0]) == 'c' && original[1] == ':'))
        return original;

    // Strategy 2: manual hardcoded prefix from INI
    if (!g_hardcodedC.empty())
    {
        std::string origLow = original, hardLow = g_hardcodedC;
        std::transform(origLow.begin(), origLow.end(), origLow.begin(), ::tolower);
        std::transform(hardLow.begin(), hardLow.end(), hardLow.begin(), ::tolower);
        if (origLow.find(hardLow) == 0)
        {
            std::string patched = g_realInstallPath + original.substr(g_hardcodedC.size());
            Log("PatchPath(manual): [" + original + "] -> [" + patched + "]");
            return patched;
        }
    }

    // Strategy 1: auto — same directory structure, different drive
    std::string realDirs = g_realInstallPath.substr(2);
    std::string fakePath = "C:" + realDirs;
    std::string origLow = original, fakeLow = fakePath;
    std::transform(origLow.begin(), origLow.end(), origLow.begin(), ::tolower);
    std::transform(fakeLow.begin(), fakeLow.end(), fakeLow.begin(), ::tolower);
    if (origLow.find(fakeLow) == 0)
    {
        std::string patched = g_realInstallPath + original.substr(fakePath.size());
        Log("PatchPath(auto):   [" + original + "] -> [" + patched + "]");
        return patched;
    }

    return original;
}

static std::string WideToNarrow(const std::wstring& w)
{
    if (w.empty()) return "";
    int sz = WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(sz, '\0');
    WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, s.data(), sz, nullptr, nullptr);
    if (!s.empty()) s.pop_back();
    return s;
}

static std::wstring PatchPathW(const std::wstring& original)
{
    if (original.empty()) return original;
    std::string narrow = WideToNarrow(original);
    std::string patched = PatchPath(narrow);
    if (patched == narrow) return original;
    int wsz = MultiByteToWideChar(CP_ACP, 0, patched.c_str(), -1, nullptr, 0);
    std::wstring wide(wsz, L'\0');
    MultiByteToWideChar(CP_ACP, 0, patched.c_str(), -1, wide.data(), wsz);
    wide.pop_back();
    return wide;
}

// ─────────────────────────────────────────────
//  Hooked API Functions
// ─────────────────────────────────────────────

// CreateFileA/W
static HANDLE (WINAPI *Real_CreateFileA)(LPCSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE) = CreateFileA;
HANDLE WINAPI Hook_CreateFileA(LPCSTR n,DWORD a,DWORD s,LPSECURITY_ATTRIBUTES sa,DWORD c,DWORD f,HANDLE t){
    if(n){ LogTrace("CreateFileA",n); auto p=PatchPath(n); return Real_CreateFileA(p.c_str(),a,s,sa,c,f,t); }
    return Real_CreateFileA(n,a,s,sa,c,f,t);
}
static HANDLE (WINAPI *Real_CreateFileW)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE) = CreateFileW;
HANDLE WINAPI Hook_CreateFileW(LPCWSTR n,DWORD a,DWORD s,LPSECURITY_ATTRIBUTES sa,DWORD c,DWORD f,HANDLE t){
    if(n){ LogTrace("CreateFileW",WideToNarrow(n)); auto p=PatchPathW(n); return Real_CreateFileW(p.c_str(),a,s,sa,c,f,t); }
    return Real_CreateFileW(n,a,s,sa,c,f,t);
}

// Helper: logs LoadLibrary result — on failure logs the Windows error code and message
static void LogLoadLibResult(const std::string& api, const std::string& name, HMODULE hMod)
{
    if (!g_log.is_open()) return;
    if (hMod)
    {
        if (g_traceMode) Log(api + " OK [" + name + "]");
    }
    else
    {
        DWORD err = GetLastError();
        char msgBuf[512] = {};
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, err, 0, msgBuf, sizeof(msgBuf), nullptr);
        // Strip trailing newline from FormatMessage
        for (int i = (int)strlen(msgBuf)-1; i >= 0 && (msgBuf[i]=='\r'||msgBuf[i]=='\n'); --i)
            msgBuf[i] = '\0';
        Log("LoadLibrary FAILED [" + name + "] error=" + std::to_string(err) + " (" + msgBuf + ")");
    }
}

// LoadLibraryA/W/ExA/ExW
static HMODULE (WINAPI *Real_LoadLibraryA)(LPCSTR) = LoadLibraryA;
HMODULE WINAPI Hook_LoadLibraryA(LPCSTR n){
    if(n){
        LogTrace("LoadLibraryA",n);
        auto p=PatchPath(n);
        HMODULE h = Real_LoadLibraryA(p.c_str());
        LogLoadLibResult("LoadLibraryA", p, h);
        return h;
    }
    return Real_LoadLibraryA(n);
}
static HMODULE (WINAPI *Real_LoadLibraryW)(LPCWSTR) = LoadLibraryW;
HMODULE WINAPI Hook_LoadLibraryW(LPCWSTR n){
    if(n){
        LogTrace("LoadLibraryW",WideToNarrow(n));
        auto p=PatchPathW(n);
        HMODULE h = Real_LoadLibraryW(p.c_str());
        LogLoadLibResult("LoadLibraryW", WideToNarrow(p), h);
        return h;
    }
    return Real_LoadLibraryW(n);
}
static HMODULE (WINAPI *Real_LoadLibraryExA)(LPCSTR,HANDLE,DWORD) = LoadLibraryExA;
HMODULE WINAPI Hook_LoadLibraryExA(LPCSTR n,HANDLE h,DWORD f){
    if(n){
        LogTrace("LoadLibraryExA",n);
        auto p=PatchPath(n);
        HMODULE hMod = Real_LoadLibraryExA(p.c_str(),h,f);
        LogLoadLibResult("LoadLibraryExA", p, hMod);
        return hMod;
    }
    return Real_LoadLibraryExA(n,h,f);
}
static HMODULE (WINAPI *Real_LoadLibraryExW)(LPCWSTR,HANDLE,DWORD) = LoadLibraryExW;
HMODULE WINAPI Hook_LoadLibraryExW(LPCWSTR n,HANDLE h,DWORD f){
    if(n){
        LogTrace("LoadLibraryExW",WideToNarrow(n));
        auto p=PatchPathW(n);
        HMODULE hMod = Real_LoadLibraryExW(p.c_str(),h,f);
        LogLoadLibResult("LoadLibraryExW", WideToNarrow(p), hMod);
        return hMod;
    }
    return Real_LoadLibraryExW(n,h,f);
}

// FindFirstFileA/W
static HANDLE (WINAPI *Real_FindFirstFileA)(LPCSTR,LPWIN32_FIND_DATAA) = FindFirstFileA;
HANDLE WINAPI Hook_FindFirstFileA(LPCSTR n,LPWIN32_FIND_DATAA d){
    if(n){ LogTrace("FindFirstFileA",n); auto p=PatchPath(n); return Real_FindFirstFileA(p.c_str(),d); }
    return Real_FindFirstFileA(n,d);
}
static HANDLE (WINAPI *Real_FindFirstFileW)(LPCWSTR,LPWIN32_FIND_DATAW) = FindFirstFileW;
HANDLE WINAPI Hook_FindFirstFileW(LPCWSTR n,LPWIN32_FIND_DATAW d){
    if(n){ LogTrace("FindFirstFileW",WideToNarrow(n)); auto p=PatchPathW(n); return Real_FindFirstFileW(p.c_str(),d); }
    return Real_FindFirstFileW(n,d);
}

// GetFileAttributesA/W
// Special case: if the game checks "C:" or "C:\" bare root (drive existence check),
// we return INVALID_FILE_ATTRIBUTES to make the game believe C: doesn't exist.
// This prevents the game from entering its "I should be on C:" inconsistency logic.
static bool IsBareC(const std::string& s){
    if(s.size()<2) return false;
    char d=std::tolower((unsigned char)s[0]);
    if(d!='c' || s[1]!=':') return false;
    // accept "C:", "C:\", "C:/" only
    return s.size()==2 || ((s.size()==3) && (s[2]=='\\'||s[2]=='/'));
}
static DWORD (WINAPI *Real_GetFileAttributesA)(LPCSTR) = GetFileAttributesA;
DWORD WINAPI Hook_GetFileAttributesA(LPCSTR n){
    if(n){
        LogTrace("GetFileAttributesA",n);
        if(IsBareC(n)){
            Log("GetFileAttributesA: hiding C: root -> INVALID");
            SetLastError(ERROR_FILE_NOT_FOUND);
            return INVALID_FILE_ATTRIBUTES;
        }
        auto p=PatchPath(n); return Real_GetFileAttributesA(p.c_str());
    }
    return Real_GetFileAttributesA(n);
}
static DWORD (WINAPI *Real_GetFileAttributesW)(LPCWSTR) = GetFileAttributesW;
DWORD WINAPI Hook_GetFileAttributesW(LPCWSTR n){
    if(n){
        std::string narrow=WideToNarrow(n);
        LogTrace("GetFileAttributesW",narrow);
        if(IsBareC(narrow)){
            Log("GetFileAttributesW: hiding C: root -> INVALID");
            SetLastError(ERROR_FILE_NOT_FOUND);
            return INVALID_FILE_ATTRIBUTES;
        }
        auto p=PatchPathW(n); return Real_GetFileAttributesW(p.c_str());
    }
    return Real_GetFileAttributesW(n);
}

// GetPrivateProfileStringA/W
static DWORD (WINAPI *Real_GetPrivateProfileStringA)(LPCSTR,LPCSTR,LPCSTR,LPSTR,DWORD,LPCSTR) = GetPrivateProfileStringA;
DWORD WINAPI Hook_GetPrivateProfileStringA(LPCSTR a,LPCSTR k,LPCSTR d,LPSTR r,DWORD n,LPCSTR f){
    if(f){ LogTrace("GetPrivateProfileStringA",f); auto p=PatchPath(f); return Real_GetPrivateProfileStringA(a,k,d,r,n,p.c_str()); }
    return Real_GetPrivateProfileStringA(a,k,d,r,n,f);
}
static DWORD (WINAPI *Real_GetPrivateProfileStringW)(LPCWSTR,LPCWSTR,LPCWSTR,LPWSTR,DWORD,LPCWSTR) = GetPrivateProfileStringW;
DWORD WINAPI Hook_GetPrivateProfileStringW(LPCWSTR a,LPCWSTR k,LPCWSTR d,LPWSTR r,DWORD n,LPCWSTR f){
    if(f){ LogTrace("GetPrivateProfileStringW",WideToNarrow(f)); auto p=PatchPathW(f); return Real_GetPrivateProfileStringW(a,k,d,r,n,p.c_str()); }
    return Real_GetPrivateProfileStringW(a,k,d,r,n,f);
}

// RegOpenKeyExA
static LSTATUS (WINAPI *Real_RegOpenKeyExA)(HKEY,LPCSTR,DWORD,REGSAM,PHKEY) = RegOpenKeyExA;
LSTATUS WINAPI Hook_RegOpenKeyExA(HKEY h,LPCSTR k,DWORD o,REGSAM s,PHKEY r){
    if(k){ LogTrace("RegOpenKeyExA",k); auto p=PatchPath(k); return Real_RegOpenKeyExA(h,p.c_str(),o,s,r); }
    return Real_RegOpenKeyExA(h,k,o,s,r);
}

// ─────────────────────────────────────────────
//  Synchronisation hooks (DRM / event detection)
// ─────────────────────────────────────────────

// OpenEventA — intercept so we can see what named events the game/DRM waits on.
// If the event doesn't exist (DRM not running), the game loops forever.
// With FakeEvents=1 in the INI we create a dummy manual-reset event so the
// game's OpenEventA succeeds and execution can continue past the DRM check.
static bool g_fakeEvents = false;

static HANDLE (WINAPI *Real_OpenEventA)(DWORD,BOOL,LPCSTR) = OpenEventA;
HANDLE WINAPI Hook_OpenEventA(DWORD dwAccess, BOOL bInherit, LPCSTR lpName)
{
    if (lpName)
    {
        HANDLE h = Real_OpenEventA(dwAccess, bInherit, lpName);
        if (h)
        {
            Log("OpenEventA OK    [" + std::string(lpName) + "]");
        }
        else
        {
            DWORD err = GetLastError();
            Log("OpenEventA FAIL  [" + std::string(lpName) + "] err=" + std::to_string(err));
            if (g_fakeEvents)
            {
                // Create a signalled manual-reset event so any WaitForSingleObject
                // on it returns immediately instead of hanging.
                h = CreateEventA(nullptr, TRUE, TRUE, lpName);
                if (h) Log("OpenEventA FAKED [" + std::string(lpName) + "] (signalled)");
            }
        }
        return h;
    }
    return Real_OpenEventA(dwAccess, bInherit, lpName);
}

// WaitForSingleObject — log every wait so we can spot which handle hangs.
// Only active in TraceMode to avoid log spam during normal play.
static DWORD (WINAPI *Real_WaitForSingleObject)(HANDLE,DWORD) = WaitForSingleObject;
DWORD WINAPI Hook_WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds)
{
    if (g_traceMode)
    {
        std::string timeout = (dwMilliseconds == INFINITE) ? "INFINITE" : std::to_string(dwMilliseconds) + "ms";
        Log("WaitForSingleObject handle=" + std::to_string(reinterpret_cast<uintptr_t>(hHandle))
            + " timeout=" + timeout);
    }
    DWORD r = Real_WaitForSingleObject(hHandle, dwMilliseconds);
    if (g_traceMode)
    {
        std::string res = (r==WAIT_OBJECT_0) ? "SIGNALLED" :
                          (r==WAIT_TIMEOUT)  ? "TIMEOUT"   :
                          (r==WAIT_FAILED)   ? "FAILED"    : std::to_string(r);
        Log("WaitForSingleObject result=" + res);
    }
    return r;
}

// GetCommandLineA — log the command line the game sees (may contain install path).
static LPSTR (WINAPI *Real_GetCommandLineA)() = GetCommandLineA;
LPSTR WINAPI Hook_GetCommandLineA()
{
    LPSTR cmd = Real_GetCommandLineA();
    if (cmd) LogTrace("GetCommandLineA", cmd);
    return cmd;
}

// GetModuleFileNameA — if the game checks its own path and expects C:\, patch it.
static DWORD (WINAPI *Real_GetModuleFileNameA)(HMODULE,LPSTR,DWORD) = GetModuleFileNameA;
DWORD WINAPI Hook_GetModuleFileNameA(HMODULE hMod, LPSTR lpFilename, DWORD nSize)
{
    DWORD r = Real_GetModuleFileNameA(hMod, lpFilename, nSize);
    if (r > 0 && lpFilename)
    {
        LogTrace("GetModuleFileNameA", lpFilename);
        std::string patched = PatchPath(lpFilename);
        if (patched != std::string(lpFilename))
        {
            Log("GetModuleFileNameA patch: [" + std::string(lpFilename) + "] -> [" + patched + "]");
            strncpy_s(lpFilename, nSize, patched.c_str(), _TRUNCATE);
            r = (DWORD)patched.size();
        }
    }
    return r;
}

// RegQueryValueExA — patches the VALUE returned, not just the key name.
// Two-pass: first read into our own buffer, patch, then copy back.
// This handles the case where the patched path is LONGER than the original.
static LSTATUS (WINAPI *Real_RegQueryValueExA)(HKEY,LPCSTR,LPDWORD,LPDWORD,LPBYTE,LPDWORD) = RegQueryValueExA;
LSTATUS WINAPI Hook_RegQueryValueExA(HKEY h,LPCSTR n,LPDWORD res,LPDWORD type,LPBYTE data,LPDWORD cb){
    DWORD realType=0, realSize=0;
    LSTATUS probe = Real_RegQueryValueExA(h,n,res,&realType,nullptr,&realSize);
    if((probe!=ERROR_SUCCESS && probe!=ERROR_MORE_DATA) ||
       (realType!=REG_SZ && realType!=REG_EXPAND_SZ))
        return Real_RegQueryValueExA(h,n,res,type,data,cb);
    std::string tmp(realSize,'\0');
    LSTATUS r = Real_RegQueryValueExA(h,n,res,&realType,reinterpret_cast<LPBYTE>(tmp.data()),&realSize);
    if(r!=ERROR_SUCCESS) return r;
    if(realSize>0) tmp.resize(realSize-1);
    LogTrace("RegQueryValueExA(ret)", tmp);
    std::string patched = PatchPath(tmp);
    if(patched!=tmp)
        Log("RegQueryValueExA patch value: ["+tmp+"] -> ["+patched+"]");
    if(type)  *type = realType;
    DWORD needed = (DWORD)(patched.size()+1);
    if(!cb)   return ERROR_SUCCESS;
    if(!data) { *cb=needed; return ERROR_SUCCESS; }
    if(*cb<needed){ *cb=needed; return ERROR_MORE_DATA; }
    memcpy(data, patched.c_str(), needed);
    *cb = needed;
    return ERROR_SUCCESS;
}

// ─────────────────────────────────────────────
//  Init / Shutdown
// ─────────────────────────────────────────────

static void Init()
{
    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::filesystem::path p(exePath);
    g_realInstallPath = p.parent_path().string();
    while (!g_realInstallPath.empty() &&
           (g_realInstallPath.back()=='\\' || g_realInstallPath.back()=='/'))
        g_realInstallPath.pop_back();

    std::string iniPath = p.parent_path().string() + "\\GhostReconFS-PathFix.ini";
    g_debugMode  = IsTruthy(LoadIniKey(iniPath, "debugmode"));
    g_traceMode  = IsTruthy(LoadIniKey(iniPath, "tracemode"));
    g_hardcodedC = LoadIniKey(iniPath, "hardcodedpath");
    g_fakeEvents = IsTruthy(LoadIniKey(iniPath, "fakeevents"));
    while (!g_hardcodedC.empty() &&
           (g_hardcodedC.back()=='\\' || g_hardcodedC.back()=='/'))
        g_hardcodedC.pop_back();

    if (g_debugMode || g_traceMode)
    {
        std::string logPath = p.parent_path().string() + "\\GhostReconFS-PathFix.log";
        g_log.open(logPath, std::ios::out | std::ios::trunc);
    }

    Log("=== GhostReconFS-PathFix loaded ===");
    Log("Real install path : " + g_realInstallPath);
    Log("Debug mode        : " + std::string(g_debugMode ? "ON" : "OFF"));
    Log("Trace mode        : " + std::string(g_traceMode ? "ON (ALL paths logged)" : "OFF"));
    Log("Hardcoded path    : " + (g_hardcodedC.empty() ? "(auto)" : g_hardcodedC));
    Log("Fake events       : " + std::string(g_fakeEvents ? "ON" : "OFF"));
    Log("INI path          : " + iniPath);

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
    DetourAttach(&(PVOID&)Real_CreateFileA,              Hook_CreateFileA);
    DetourAttach(&(PVOID&)Real_CreateFileW,              Hook_CreateFileW);
    DetourAttach(&(PVOID&)Real_LoadLibraryA,             Hook_LoadLibraryA);
    DetourAttach(&(PVOID&)Real_LoadLibraryW,             Hook_LoadLibraryW);
    DetourAttach(&(PVOID&)Real_LoadLibraryExA,           Hook_LoadLibraryExA);
    DetourAttach(&(PVOID&)Real_LoadLibraryExW,           Hook_LoadLibraryExW);
    DetourAttach(&(PVOID&)Real_FindFirstFileA,           Hook_FindFirstFileA);
    DetourAttach(&(PVOID&)Real_FindFirstFileW,           Hook_FindFirstFileW);
    DetourAttach(&(PVOID&)Real_GetFileAttributesA,       Hook_GetFileAttributesA);
    DetourAttach(&(PVOID&)Real_GetFileAttributesW,       Hook_GetFileAttributesW);
    DetourAttach(&(PVOID&)Real_GetPrivateProfileStringA, Hook_GetPrivateProfileStringA);
    DetourAttach(&(PVOID&)Real_GetPrivateProfileStringW, Hook_GetPrivateProfileStringW);
    DetourAttach(&(PVOID&)Real_RegOpenKeyExA,            Hook_RegOpenKeyExA);
    DetourAttach(&(PVOID&)Real_RegQueryValueExA,         Hook_RegQueryValueExA);
    DetourAttach(&(PVOID&)Real_OpenEventA,               Hook_OpenEventA);
    DetourAttach(&(PVOID&)Real_WaitForSingleObject,      Hook_WaitForSingleObject);
    DetourAttach(&(PVOID&)Real_GetCommandLineA,           Hook_GetCommandLineA);
    DetourAttach(&(PVOID&)Real_GetModuleFileNameA,        Hook_GetModuleFileNameA);
    LONG result = DetourTransactionCommit();
    Log(std::string("DetourTransactionCommit result: ") +
        (result == NO_ERROR ? "OK" : "FAILED (" + std::to_string(result) + ")"));
}

static void Shutdown()
{
    Log("Removing hooks...");
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID&)Real_CreateFileA,              Hook_CreateFileA);
    DetourDetach(&(PVOID&)Real_CreateFileW,              Hook_CreateFileW);
    DetourDetach(&(PVOID&)Real_LoadLibraryA,             Hook_LoadLibraryA);
    DetourDetach(&(PVOID&)Real_LoadLibraryW,             Hook_LoadLibraryW);
    DetourDetach(&(PVOID&)Real_LoadLibraryExA,           Hook_LoadLibraryExA);
    DetourDetach(&(PVOID&)Real_LoadLibraryExW,           Hook_LoadLibraryExW);
    DetourDetach(&(PVOID&)Real_FindFirstFileA,           Hook_FindFirstFileA);
    DetourDetach(&(PVOID&)Real_FindFirstFileW,           Hook_FindFirstFileW);
    DetourDetach(&(PVOID&)Real_GetFileAttributesA,       Hook_GetFileAttributesA);
    DetourDetach(&(PVOID&)Real_GetFileAttributesW,       Hook_GetFileAttributesW);
    DetourDetach(&(PVOID&)Real_GetPrivateProfileStringA, Hook_GetPrivateProfileStringA);
    DetourDetach(&(PVOID&)Real_GetPrivateProfileStringW, Hook_GetPrivateProfileStringW);
    DetourDetach(&(PVOID&)Real_RegOpenKeyExA,            Hook_RegOpenKeyExA);
    DetourDetach(&(PVOID&)Real_RegQueryValueExA,         Hook_RegQueryValueExA);
    DetourDetach(&(PVOID&)Real_OpenEventA,               Hook_OpenEventA);
    DetourDetach(&(PVOID&)Real_WaitForSingleObject,      Hook_WaitForSingleObject);
    DetourDetach(&(PVOID&)Real_GetCommandLineA,           Hook_GetCommandLineA);
    DetourDetach(&(PVOID&)Real_GetModuleFileNameA,        Hook_GetModuleFileNameA);
    DetourTransactionCommit();
    Log("=== GhostReconFS-PathFix unloaded ===");
    if (g_log.is_open()) g_log.close();
}

// ─────────────────────────────────────────────
//  DLL Entry Point
// ─────────────────────────────────────────────

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH: DisableThreadLibraryCalls(hModule); Init();     break;
        case DLL_PROCESS_DETACH:                                      Shutdown(); break;
    }
    return TRUE;
}
