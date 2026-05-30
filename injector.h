#pragma once
#include <Windows.h>
#include <string>
#include <vector>

struct procInfo
{
    DWORD pid;
    std::string name;
    std::string arch;
};

namespace injector
{
std::vector<procInfo> getProcs();

std::string injectLoadLibrary(DWORD pID, const std::string &dllPath);
std::string injectApc(DWORD pID, const std::string &dllPath);
std::string injectThreadHijack(DWORD pID, const std::string &dllPath);

std::string injectBlackBone(DWORD pID, const std::string &dllPath, bool erasePE, bool hideModule);

std::string stealthInject(DWORD pID, const std::string &dllPath);
} // namespace injector
