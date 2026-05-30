#include "injector.h"
#include <tlhelp32.h>
#include <stdexcept>
#include <vector>
#include <memory>
#include <string>

#include <BlackBone/Process/Process.h>
#include <BlackBone/Patterns/PatternSearch.h>
#include <BlackBone/Process/RPC/RemoteFunction.hpp>
#include <BlackBone/Syscalls/Syscall.h>
#include <BlackBone/Misc/Utils.h>
#include "BlackBone/Config.h"
#include "BlackBone/DriverControl/DriverControl.h"
#include "BlackBone/Asm/AsmFactory.h"



#pragma comment(lib, "BlackBone.lib")


static std::wstring s2ws(const std::string& s) {
    int len;
    int slength = (int)s.length() + 1;
    len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
    wchar_t* buf = new wchar_t[len];
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
    std::wstring r(buf);
    delete[] buf;
    return r;
}


static std::string ws2s(const std::wstring& w_str) {
    if (w_str.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_ACP, 0, &w_str[0], (int)w_str.size(), NULL, 0, NULL, NULL);
    std::string str_to(size_needed, 0);
    WideCharToMultiByte(CP_ACP, 0, &w_str[0], (int)w_str.size(), &str_to[0], size_needed, NULL, NULL);
    return str_to;
}


namespace injector
{


    static std::string loadDriver() {

        NTSTATUS status = blackbone::Driver().EnsureLoaded();
        if (NT_SUCCESS(status)) {
            return "";
        }


        switch (status) {
        case 0xC0000034:
            return "driver load failed: .sys file not found. make sure it's next to the exe.";
        case 0xC0000022:
            return "driver load failed: access denied. please run the injector as an administrator.";
        default:
            return "driver load failed with unknown error code: " + std::to_string(status);
        }
    }

    std::vector<procInfo> getProcs()
    {
        std::vector<procInfo> procs;
        auto result = blackbone::Process::EnumByNameOrPID(0, L"");

        if (result)
        {
            for (const auto& p : result.result())
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
                    info.arch = "N/A";
                }

                procs.push_back(info);
            }
        }

        return procs;
    }

    std::string injectLoadLibrary(DWORD pID, const std::string& dllPath)
    {
        blackbone::Process proc;
        if (!NT_SUCCESS(proc.Attach(pID))) {
            return "loadlibrary failed: could not attach to process.";
        }


        auto result = proc.modules().Inject(s2ws(dllPath));
        if (!result) {
            return "loadlibrary injection failed. status: " + std::to_string(result.status);
        }

        proc.Detach();
        return "loadlibrary injection successful!";
    }

    std::string injectApc(DWORD pID, const std::string& dllPath)
    {
        blackbone::Process proc;
        NTSTATUS status = proc.Attach(pID);
        if (!NT_SUCCESS(status)) {
            return "apc failed: could not attach to process. status: " + std::to_string(status);
        }

        int count = 0;
        std::string errorMsg = "";


        {

            auto mod = proc.modules().GetModule(L"kernel32.dll");
            if (!mod) {
                errorMsg = "apc failed: could not find kernel32.dll.";
                goto end_apc;
            }
            auto pLoadLibraryW = proc.modules().GetExport(mod, "LoadLibraryW");
            if (!pLoadLibraryW) {
                errorMsg = "apc failed: could not find LoadLibraryW export.";
                goto end_apc;
            }


            std::wstring dllPathW = s2ws(dllPath);
            auto mem = proc.memory().Allocate((dllPathW.length() + 1) * sizeof(wchar_t), PAGE_READWRITE);
            if (!mem) {
                errorMsg = "apc failed: could not allocate memory for dll path.";
                goto end_apc;
            }


            status = mem.result().Write(0, (dllPathW.length() + 1) * sizeof(wchar_t), dllPathW.c_str());
            if (!NT_SUCCESS(status)) {
                errorMsg = "apc failed: could not write dll path to memory. status: " + std::to_string(status);
                goto end_apc;
            }


            auto threads = proc.threads().getAll();
            if (threads.empty()) {
                errorMsg = "apc failed: could not enumerate threads.";
                goto end_apc;
            }


            for (auto& pThread : threads) {
                if (pThread && pThread->valid()) {
                    proc.core().native()->QueueApcT(pThread->handle(), pLoadLibraryW->procAddress, mem.result().ptr());
                    count++;
                }
            }
        }

    end_apc:
        proc.Detach();

        if (!errorMsg.empty()) {
            return errorMsg;
        }

        if (count > 0) {
            return "apc successful: queued to " + std::to_string(count) + " threads.";
        }

        return "apc failed: no valid threads found to queue to.";
    }

    std::string injectThreadHijack(DWORD pID, const std::string& dllPath)
    {
        blackbone::Process proc;
        NTSTATUS status = proc.Attach(pID);
        if (!NT_SUCCESS(status)) {
            return "hijack failed: could not attach to process.";
        }


        auto mod = proc.modules().GetModule(L"kernel32.dll");
        if (!mod) {
            proc.Detach();
            return "hijack failed: could not find kernel32.dll.";
        }
        auto pLoadLibraryW = proc.modules().GetExport(mod, "LoadLibraryW");
        if (!pLoadLibraryW) {
            proc.Detach();
            return "hijack failed: could not find LoadLibraryW export.";
        }


        blackbone::RemoteFunction<decltype(&LoadLibraryW)> remoteLoadLibrary(proc, pLoadLibraryW->procAddress);


        auto pThread = proc.threads().getRandom();
        if (!pThread || !pThread->valid()) {
            proc.Detach();
            return "hijack failed: could not find a valid thread to hijack.";
        }


        auto result = remoteLoadLibrary.Call({ s2ws(dllPath).c_str() }, pThread);

        proc.Detach();

        if (result.success()) {

            if (result.result() != 0) {
                return "thread hijack successful!";
            }
            else {
                return "hijack failed: LoadLibrary returned null (check DLL bits or dependencies).";
            }
        }
        else {
            return "hijack failed: remote call failed. status: " + std::to_string(result.status);
        }
    }

    std::string injectBlackBone(DWORD pID, const std::string& dllPath, bool erasePE, bool hideModule)
    {
        std::string driver_err = loadDriver();
        if (!driver_err.empty()) {
            return driver_err;
        }

        blackbone::Process proc;
        if (!NT_SUCCESS(proc.Attach(pID))) {
            return "blackbone: failed to attach to process.";
        }

        blackbone::eLoadFlags flags = blackbone::CreateLdrRef;
        if (erasePE) {
            flags |= blackbone::WipeHeader;
        }
        if (hideModule) {
            flags |= blackbone::HideVAD;
        }

        auto result = proc.mmap().MapImage(s2ws(dllPath), flags);

        if (!result) {
            proc.Detach();
            return "blackbone injection failed. status: " + std::to_string(result.status);
        }

        proc.Detach();
        return "blackbone injection successful!";
    }

    std::string stealthInject(DWORD pID, const std::string& dllPath)
    {
        std::string driver_err = loadDriver();
        if (!driver_err.empty()) {
            return driver_err;
        }

        blackbone::Process proc;
        if (!NT_SUCCESS(proc.Attach(pID))) {
            return "stealth inject: failed to attach to process.";
        }

        auto flags = blackbone::WipeHeader | blackbone::HideVAD | blackbone::ManualImports;

        auto result = proc.mmap().MapImage(s2ws(dllPath), flags);

        if (!result) {
            proc.Detach();
            return "stealth injection failed. status: " + std::to_string(result.status);
        }

        proc.Detach();
        return "stealth injection successful!";
    }
}
