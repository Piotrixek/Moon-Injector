#include "injector.h"
#include <tlhelp32.h>
#include <stdexcept>
#include <vector>

std::vector<procInfo> injector::getProcs() {
    std::vector<procInfo> procs;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        return procs;
    }

    PROCESSENTRY32 procEntry;
    procEntry.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnap, &procEntry)) {
        do {
            procInfo currentProc;
            currentProc.pid = procEntry.th32ProcessID;
            currentProc.name = procEntry.szExeFile;

            // check the arch
            BOOL isWow64 = FALSE;
            HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, currentProc.pid);
            if (hProc) {
                if (IsWow64Process(hProc, &isWow64)) {
                    currentProc.arch = isWow64 ? "x86" : "x64";
                } else {
                    currentProc.arch = "N/A";
                }
                CloseHandle(hProc);
            } else {
                 currentProc.arch = "N/A";
            }
            procs.push_back(currentProc);
        } while (Process32Next(hSnap, &procEntry));
    }

    CloseHandle(hSnap);
    return procs;
}

std::string injector::inject(DWORD pID, const std::string& dllPath) {
    if (dllPath.empty() || pID == 0) {
        return "Invalid process ID or DLL path.";
    }

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pID);
    if (!hProc || hProc == INVALID_HANDLE_VALUE) {
        return "Failed to open process. Maybe run as admin?";
    }

    // get address for loadlibrarya in kernel32. its the same in all procs
    LPVOID loadLibAddr = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    if (!loadLibAddr) {
        CloseHandle(hProc);
        return "Failed to get LoadLibraryA address.";
    }

    // alloc mem in the target proc for our dll path
    LPVOID remoteStr = VirtualAllocEx(hProc, NULL, dllPath.length() + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteStr) {
        CloseHandle(hProc);
        return "Failed to allocate memory in target process.";
    }

    // write the dll path to that new memory
    if (!WriteProcessMemory(hProc, remoteStr, dllPath.c_str(), dllPath.length() + 1, NULL)) {
        VirtualFreeEx(hProc, remoteStr, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return "Failed to write DLL path to process memory.";
    }

    // create a thread in the target proc that calls loadlibrarya with our dll path as the argument
    HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibAddr, remoteStr, 0, NULL);
    if (!hThread) {
        VirtualFreeEx(hProc, remoteStr, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return "Failed to create remote thread.";
    }

    WaitForSingleObject(hThread, INFINITE);
    
    // check if the thread actually worked
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);

    // cleanup
    CloseHandle(hThread);
    VirtualFreeEx(hProc, remoteStr, 0, MEM_RELEASE);
    CloseHandle(hProc);

    if (exitCode == 0) {
        return "Remote thread failed to load library (returned 0).";
    }

    return "Successfully injected!";
}