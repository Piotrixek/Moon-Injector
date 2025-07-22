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


// Link against the Blackbone library
#pragma comment(lib, "BlackBone.lib")

// --- Helper function to convert string to wstring ---
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

// --- Helper function to convert wstring to string ---
static std::string ws2s(const std::wstring& w_str) {
    if (w_str.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_ACP, 0, &w_str[0], (int)w_str.size(), NULL, 0, NULL, NULL);
    std::string str_to(size_needed, 0);
    WideCharToMultiByte(CP_ACP, 0, &w_str[0], (int)w_str.size(), &str_to[0], size_needed, NULL, NULL);
    return str_to;
}


namespace injector
{
    // Helper function to load the BlackBone driver
    // Returns an empty string on success, or an error message on failure.
    static std::string loadDriver() {
        // The driver file (e.g., BlackBoneDrv10.sys) should be in the same directory as the executable
        NTSTATUS status = blackbone::Driver().EnsureLoaded();
        if (NT_SUCCESS(status)) {
            return ""; // Success
        }

        // Provide a more descriptive error message
        switch (status) {
        case 0xC0000034: // STATUS_OBJECT_NAME_NOT_FOUND
            return "driver load failed: .sys file not found. make sure it's next to the exe.";
        case 0xC0000022: // STATUS_ACCESS_DENIED
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

        // Use BlackBone's implementation for consistency
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

        // Create a new scope to ensure MemBlock is destructed before proc.Detach()
        {
            // Get kernel32.dll and the functions we need
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

            // Allocate memory for the DLL path in the target process
            std::wstring dllPathW = s2ws(dllPath);
            auto mem = proc.memory().Allocate((dllPathW.length() + 1) * sizeof(wchar_t), PAGE_READWRITE);
            if (!mem) {
                errorMsg = "apc failed: could not allocate memory for dll path.";
                goto end_apc;
            }

            // Write the DLL path into the allocated memory
            status = mem.result().Write(0, (dllPathW.length() + 1) * sizeof(wchar_t), dllPathW.c_str());
            if (!NT_SUCCESS(status)) {
                errorMsg = "apc failed: could not write dll path to memory. status: " + std::to_string(status);
                goto end_apc;
            }

            // Get all threads in the process
            auto threads = proc.threads().getAll();
            if (threads.empty()) {
                errorMsg = "apc failed: could not enumerate threads.";
                goto end_apc;
            }

            // Queue the APC to each valid thread
            for (auto& pThread : threads) {
                if (pThread && pThread->valid()) {
                    proc.core().native()->QueueApcT(pThread->handle(), pLoadLibraryW->procAddress, mem.result().ptr());
                    count++;
                }
            }
        } // mem's destructor is called here, freeing the memory while proc is still attached

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

        std::string hijackResult = "";

        // Create a new scope to ensure all MemBlocks are destructed before proc.Detach()
        {
            auto pThread = proc.threads().getRandom();
            if (!pThread || !pThread->valid()) {
                hijackResult = "hijack failed: could not find a valid thread to hijack.";
                goto end_hijack;
            }

            auto mod = proc.modules().GetModule(L"kernel32.dll");
            if (!mod) {
                hijackResult = "hijack failed: could not find kernel32.dll.";
                goto end_hijack;
            }
            auto pLoadLibraryW = proc.modules().GetExport(mod, "LoadLibraryW");
            if (!pLoadLibraryW) {
                hijackResult = "hijack failed: could not find LoadLibraryW export.";
                goto end_hijack;
            }

            // Suspend the thread before we start messing with it
            if (!pThread->Suspend()) {
                hijackResult = "hijack failed: could not suspend thread.";
                goto end_hijack;
            }

            std::wstring dllPathW = s2ws(dllPath);
            auto pathMem = proc.memory().Allocate((dllPathW.length() + 1) * sizeof(wchar_t), PAGE_READWRITE);
            if (!pathMem) {
                pThread->Resume();
                hijackResult = "hijack failed: could not allocate memory for dll path.";
                goto end_hijack;
            }
            pathMem.result().Write(0, (dllPathW.length() + 1) * sizeof(wchar_t), dllPathW.c_str());

            auto asmPtr = blackbone::AsmFactory::GetAssembler(proc.core().isWow64());
            auto& a = *asmPtr;
            auto& assembler = *a.assembler(); // Get the underlying AsmJit assembler

            if (proc.core().isWow64()) {
                blackbone::_CONTEXT32 ctx;
                pThread->GetContext(ctx, CONTEXT_ALL);

                assembler.pusha();
                assembler.pushf();
                a.GenCall(pLoadLibraryW->procAddress, { pathMem.result().ptr() });
                assembler.popf();
                assembler.popa();
                assembler.push(ctx.Eip);
                assembler.ret();

                auto codeMem = proc.memory().Allocate(a->getCodeSize(), PAGE_EXECUTE_READWRITE);
                if (!codeMem) {
                    pThread->Resume();
                    hijackResult = "hijack failed: could not allocate memory for shellcode.";
                    goto end_hijack;
                }
                codeMem.result().Write(0, a->getCodeSize(), a->make());
                ctx.Eip = static_cast<uint32_t>(codeMem.result().ptr());
                pThread->SetContext(ctx);

            }
            else {
                blackbone::_CONTEXT64 ctx;
                pThread->GetContext(ctx, CONTEXT_ALL);

                // x64 needs a "shadow space" on the stack
                assembler.sub(asmjit::x86::rsp, 40);
                a.GenCall(pLoadLibraryW->procAddress, { pathMem.result().ptr() });
                assembler.add(asmjit::x86::rsp, 40);
                assembler.mov(asmjit::x86::rax, ctx.Rip);
                assembler.push(asmjit::x86::rax);
                assembler.ret();

                auto codeMem = proc.memory().Allocate(a->getCodeSize(), PAGE_EXECUTE_READWRITE);
                if (!codeMem) {
                    pThread->Resume();
                    hijackResult = "hijack failed: could not allocate memory for shellcode.";
                    goto end_hijack;
                }
                codeMem.result().Write(0, a->getCodeSize(), a->make());
                ctx.Rip = codeMem.result().ptr();
                pThread->SetContext(ctx);
            }

            pThread->Resume();
            hijackResult = "thread hijack successful!";
        } // All MemBlocks are destructed here

    end_hijack:
        proc.Detach();
        return hijackResult;
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
