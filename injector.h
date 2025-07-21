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
    std::string inject(DWORD pID, const std::string& dllPath);
}