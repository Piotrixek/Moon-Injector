#include "injector.h"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <tlhelp32.h>
#include <vector>
#include <windows.h>

#include "BlackBone/Asm/AsmFactory.h"
#include "BlackBone/Config.h"
#include "BlackBone/DriverControl/DriverControl.h"
#include <BlackBone/ManualMap/MMap.h>
#include <BlackBone/Misc/Utils.h>
#include <BlackBone/Patterns/PatternSearch.h>
#include <BlackBone/Process/Process.h>
#include <BlackBone/Process/ProcessModules.h>
#include <BlackBone/Process/RPC/RemoteFunction.hpp>
#include <BlackBone/Syscalls/Syscall.h>

#pragma comment(lib, "BlackBone.lib")

static std::wstring s2ws(const std::string &s)
{
    if (s.empty())
        return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

static std::string ws2s(const std::wstring &w_str)
{
    if (w_str.empty())
        return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &w_str[0], (int)w_str.size(), NULL, 0, NULL, NULL);
    std::string str_to(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &w_str[0], (int)w_str.size(), &str_to[0], size_needed, NULL, NULL);
    return str_to;
}

static bool isTestModeEnabled()
{
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll)
        return false;

    using NtQuerySystemInfoFn = NTSTATUS(WINAPI *)(ULONG, PVOID, ULONG, PULONG);
    auto pNtQuery = (NtQuerySystemInfoFn)GetProcAddress(hNtdll, "NtQuerySystemInformation");
    if (!pNtQuery)
        return false;

    struct
    {
        ULONG Length;
        ULONG CodeIntegrityOptions;
    } sci = {0};

    sci.Length = sizeof(sci);
    ULONG retLen = 0;

    NTSTATUS status = pNtQuery(103, &sci, sizeof(sci), &retLen);
    if (status >= 0)
    {
        return (sci.CodeIntegrityOptions & 0x0002) != 0;
    }
    return false;
}

namespace injector
{

static std::string loadDriver()
{
    if (!isTestModeEnabled())
    {
        return "driver load failed windows test mode must be enabled to use kernel methods";
    }

    NTSTATUS status = blackbone::Driver().EnsureLoaded();
    if (NT_SUCCESS(status))
        return "";

    if (status == 0xC0000034)
        return "driver load failed sys file not found make sure its next to the exe";
    if (status == 0xC0000022)
        return "driver load failed access denied please run the injector as an administrator";

    return "driver load failed with unknown error code " + std::to_string(status);
}

std::string professionalManualMap(DWORD pid, const std::string &dllPath)
{
    blackbone::Process proc;
    NTSTATUS status = proc.Attach(pid);
    if (!NT_SUCCESS(status))
        return "failed to attach to process id " + std::to_string(pid);

    blackbone::eLoadFlags flags = static_cast<blackbone::eLoadFlags>(blackbone::WipeHeader | blackbone::HideVAD);

    auto result = proc.mmap().MapImage(s2ws(dllPath), flags);
    if (!NT_SUCCESS(result.status))
        return "manual mapping failed status " + std::to_string(result.status);

    return "";
}

std::string standardInjection(DWORD pid, const std::string &dllPath)
{
    blackbone::Process proc;
    if (!NT_SUCCESS(proc.Attach(pid)))
        return "failed to attach to process id " + std::to_string(pid);

    auto result = proc.modules().Inject(s2ws(dllPath));
    if (!NT_SUCCESS(result.status))
        return "standard injection failed status " + std::to_string(result.status);

    return "standard injection successful";
}

std::string pureILInjection(DWORD pid, const std::string &netVersion, const std::string &dllPath,
                            const std::string &methodName, const std::string &args)
{
    blackbone::Process proc;
    if (!NT_SUCCESS(proc.Attach(pid)))
        return "failed to attach to process id " + std::to_string(pid);

    DWORD returnCode = 0;
    bool success =
        proc.modules().InjectPureIL(s2ws(netVersion), s2ws(dllPath), s2ws(methodName), s2ws(args), returnCode);
    if (!success)
        return "pure il injection failed";

    return "pure il injection successful return code " + std::to_string(returnCode);
}

std::string kernelStandardInjection(DWORD pid, const std::string &dllPath)
{
    std::string drvErr = loadDriver();
    if (!drvErr.empty())
        return drvErr;

    NTSTATUS status = blackbone::Driver().InjectDll(pid, s2ws(dllPath), IT_Thread, 0, L"", false, false, true);
    if (!NT_SUCCESS(status))
        return "kernel injection failed status " + std::to_string(status);

    return "kernel injection successful";
}

std::string kernelManualMap(DWORD pid, const std::string &dllPath)
{
    std::string drvErr = loadDriver();
    if (!drvErr.empty())
        return drvErr;

    KMmapFlags flags = static_cast<KMmapFlags>(KManualImports | KWipeHeader | KHideVAD);
    NTSTATUS status = blackbone::Driver().MmapDll(pid, s2ws(dllPath), flags);
    if (!NT_SUCCESS(status))
        return "kernel manual mapping failed status " + std::to_string(status);

    return "kernel manual mapping successful";
}

std::vector<procInfo> getProcs()
{
    std::vector<procInfo> procs;
    auto result = blackbone::Process::EnumByNameOrPID(0, L"");

    if (result)
    {
        for (const auto &p : result.result())
        {
            procInfo info;
            info.pid = p.pid;
            info.name = ws2s(p.imageName);

            blackbone::Process tempProc;
            if (NT_SUCCESS(tempProc.Attach(p.pid, PROCESS_QUERY_LIMITED_INFORMATION)))
            {
                info.arch = tempProc.core().isWow64() ? "x86" : "x64";
                tempProc.Detach();
            }
            else
            {
                info.arch = "n/a";
            }
            procs.push_back(info);
        }
    }
    return procs;
}

std::string injectApc(DWORD pID, const std::string &dllPath)
{
    blackbone::Process proc;
    if (!NT_SUCCESS(proc.Attach(pID)))
    {
        return "Failed to attach to process.";
    }

    std::wstring wDllPath = s2ws(dllPath);

    auto expData = proc.modules().GetExport(L"kernel32.dll", "LoadLibraryW");
    if (!NT_SUCCESS(expData.status))
    {
        return "Failed to find LoadLibraryW.";
    }

    size_t pathSize = (wDllPath.length() + 1) * sizeof(wchar_t);
    auto memData = proc.memory().Allocate(pathSize, PAGE_READWRITE, 0, false);

    if (!NT_SUCCESS(memData.status))
    {
        return "Memory allocation failed.";
    }

    memData->Write(0, pathSize, wDllPath.c_str());

    auto threads = proc.threads().getAll();
    if (threads.empty())
    {
        return "Process has no active threads.";
    }

    int queuedAPCs = 0;

    for (auto &thd : threads)
    {

        if (thd->Suspended())
            continue;

        NTSTATUS status = proc.core().native()->QueueApcT(thd->handle(), expData->procAddress, memData->ptr());

        if (NT_SUCCESS(status))
        {
            queuedAPCs++;
        }
    }

    if (queuedAPCs == 0)
    {
        return "Failed to queue APCs: No suitable active threads found.";
    }

    bool successfullyLoaded = false;

    for (int i = 0; i < 20; i++)
    {
        Sleep(50);

        if (proc.modules().GetModule(wDllPath) != nullptr)
        {
            successfullyLoaded = true;
            break;
        }
    }

    if (!successfullyLoaded)
    {
        return "APCs queued, but DLL did not load within timeout. Threads may not be in an alertable wait state.";
    }

    return "";
}

std::string injectThreadHijack(DWORD pID, const std::string &dllPath)
{
    blackbone::Process proc;
    if (!NT_SUCCESS(proc.Attach(pID)))
        return "failed to attach to process";

    std::wstring wDllPath = s2ws(dllPath);
    blackbone::ThreadPtr targetThread = nullptr;

    auto threads = proc.threads().getAll();
    auto mainThread = proc.threads().getMain();

    uint64_t maxExecTime = 0;

    for (auto &thd : threads)
    {

        if (mainThread && thd->id() == mainThread->id())
            continue;

        if (thd->Suspended())
            continue;

        uint64_t currentExecTime = thd->execTime();

        if (currentExecTime >= maxExecTime)
        {
            maxExecTime = currentExecTime;
            targetThread = thd;
        }
    }

    if (!targetThread)
        targetThread = mainThread;

    if (!targetThread)
        return "failed to locate a suitable thread to hijack";

    try
    {

        auto result = proc.modules().Inject(wDllPath, targetThread);

        if (!NT_SUCCESS(result.status))
            return "hijack injection failed status " + std::to_string(result.status);
    }
    catch (const std::exception &e)
    {
        return std::string("injector crashed with exception: ") + e.what();
    }
    catch (...)
    {
        return "injector crashed with unknown memory exception";
    }

    return "";
}

std::string injectBlackBone(DWORD pID, const std::string &dllPath, bool erasePE, bool hideModule)
{
    std::string driver_err = loadDriver();
    if (!driver_err.empty())
        return driver_err;

    blackbone::Process proc;
    if (!NT_SUCCESS(proc.Attach(pID)))
        return "blackbone failed to attach to process";

    blackbone::eLoadFlags flags = blackbone::CreateLdrRef;
    if (erasePE)
        flags |= blackbone::WipeHeader;
    if (hideModule)
        flags |= blackbone::HideVAD;

    auto result = proc.mmap().MapImage(s2ws(dllPath), flags);
    proc.Detach();

    if (!result)
        return "blackbone injection failed status " + std::to_string(result.status);
    return "blackbone injection successful";
}
} // namespace injector
