#include <windows.h>
#include <string>

typedef HANDLE (WINAPI* CreateFileA_t)(
    LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES,
    DWORD, DWORD, HANDLE);

CreateFileA_t originalCreateFileA;

HANDLE WINAPI hookedCreateFileA(
    LPCSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile)
{
    std::string path = lpFileName ? lpFileName : "";

    if (path.rfind("C:\\", 0) == 0) {
        std::string newPath = path;
        newPath.replace(0, 3, "D:\\Instagames\\TC_GR_Future-Soldier\\");

        return originalCreateFileA(
            newPath.c_str(),
            dwDesiredAccess,
            dwShareMode,
            lpSecurityAttributes,
            dwCreationDisposition,
            dwFlagsAndAttributes,
            hTemplateFile);
    }

    return originalCreateFileA(
        lpFileName,
        dwDesiredAccess,
        dwShareMode,
        lpSecurityAttributes,
        lpSecurityAttributes,
        dwCreationDisposition,
        dwFlagsAndAttributes,
        hTemplateFile);
}
