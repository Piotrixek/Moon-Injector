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

std::string injectApc(DWORD pID, const std::string &dllPath);
std::string injectThreadHijack(DWORD pID, const std::string &dllPath);
std::string injectBlackBone(DWORD pID, const std::string &dllPath, bool erasePE, bool hideModule);

std::string professionalManualMap(DWORD pid, const std::string &dllPath);
std::string standardInjection(DWORD pid, const std::string &dllPath);
std::string pureILInjection(DWORD pid, const std::string &netVersion, const std::string &dllPath,
                            const std::string &methodName, const std::string &args);
std::string kernelStandardInjection(DWORD pid, const std::string &dllPath);
std::string kernelManualMap(DWORD pid, const std::string &dllPath);
} // namespace injector
