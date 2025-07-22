#pragma once
#include <string>
#include <vector>
#include <Windows.h>

struct procInfo {
    DWORD pid;
    std::string name;
    std::string arch;
};

namespace injector
{
    std::vector<procInfo> getProcs();

    // --- Standard Injection Methods ---
    std::string injectLoadLibrary(DWORD pID, const std::string& dllPath);
    std::string injectApc(DWORD pID, const std::string& dllPath);
    std::string injectThreadHijack(DWORD pID, const std::string& dllPath);


    // --- Advanced Injection using BlackBone ---
    std::string injectBlackBone(DWORD pID, const std::string& dllPath, bool erasePE, bool hideModule);

    std::string stealthInject(DWORD pID, const std::string& dllPath);
}
