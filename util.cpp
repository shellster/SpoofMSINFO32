#include "util.h"

DWORD processID = -1;
HANDLE processHandle;

wchar_t* _get_msinfo32_path() {
    HMODULE hExe = GetModuleHandle(NULL);
	wchar_t system32[MAX_PATH] = { 0 };
	wchar_t * command = new wchar_t[MAX_PATH * 2 + 100];
    wchar_t* fullPath = new wchar_t[MAX_PATH];
    GetModuleFileName(hExe, fullPath, MAX_PATH);
    PathRemoveFileSpec(fullPath);


	GetSystemDirectoryW(system32, MAX_PATH);

    lstrcpyW(command, L"\"");
    lstrcatW(command, system32);
	lstrcatW(command, L"\\msinfo32.exe\" \"");
    lstrcatW(command, fullPath);
    lstrcatW(command, L"\\msinfo32.nfo\"");

	return command;
}

bool _startswith(const char* str, const char* prefix)
{
    if (strlen(prefix) > strlen(str))
        return false;

    while (*prefix)
    {
        if (*prefix != *str)
            return false;

        prefix++;
        str++;
    }

    return true;
}

bool _fixText(HANDLE process, HWND hwnd) {
    bool success = false;

    HWND hwndTree = FindWindowExA(hwnd, NULL, "SysTreeView32", NULL);

    HTREEITEM hitem = TreeView_GetSelection(hwndTree);
    if (!hitem)
        return success;

    const int buflen = 1024;

    TVITEMEXA* ptv = (TVITEMEXA*)VirtualAllocEx(process, NULL, sizeof(TVITEMEXA), MEM_COMMIT, PAGE_READWRITE);
    char* pbuf = (char*)VirtualAllocEx(process, NULL, buflen, MEM_COMMIT, PAGE_READWRITE);

    TVITEMEXA tv = { 0 };
    tv.hItem = hitem;
    tv.cchTextMax = buflen;
    tv.pszText = pbuf;
    tv.mask = TVIF_TEXT | TVIF_HANDLE;

    WriteProcessMemory(process, ptv, &tv, sizeof(TVITEMEXA), NULL);

    if (SendMessageW(hwndTree, TVM_GETITEMA, 0, (LPARAM)(TVITEMEXA*)(ptv)))
    {
        char buf[buflen];
        ReadProcessMemory(process, pbuf, buf, buflen, 0);

        const char* replacement = "System Summary";

        if (_startswith(buf, replacement)) {
            WriteProcessMemory(process, pbuf, replacement, strlen(replacement) + 1, NULL);
            SendMessageW(hwndTree, TVM_SETITEMA, 0, (LPARAM)(TVITEMEXA*)(ptv));

            success = true;
        }
    }

    VirtualFreeEx(process, ptv, 0, MEM_RELEASE);
    VirtualFreeEx(process, pbuf, 0, MEM_RELEASE);
    CloseHandle(process);

    return success;
}

BOOL CALLBACK _searchForProc(HWND hWnd, LPARAM lParam) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hWnd, &pid);

    if (pid == processID)
        _fixText(processHandle, hWnd);

    return true;
}

int startMSInfo32andFixText() {
    STARTUPINFO si = { sizeof si };
    PROCESS_INFORMATION pi = { 0 };

    wchar_t* exe_path = _get_msinfo32_path();

    if (!CreateProcess(NULL, exe_path, NULL, NULL, FALSE, CREATE_SUSPENDED | DEBUG_PROCESS, NULL, NULL, &si, &pi)) {
        return 1;
    }

    delete exe_path;

    processID = pi.dwProcessId;
    processHandle = pi.hProcess;

    // Resume the execution of the process, once all libraries have been injected
    // into its address space.
    if (ResumeThread(pi.hThread) == -1) {
        return 2;
    }

    DebugActiveProcessStop(pi.dwProcessId);

    Sleep(250);

    EnumWindows(_searchForProc, NULL);

    Sleep(3000);

    // Cleanup.
    CloseHandle(pi.hProcess);

    return 0;
}