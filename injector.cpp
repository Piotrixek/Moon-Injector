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

static std::wstring convertStringToWideString(const std::string &sourceString)
{
    if (sourceString.empty())
    {
        return std::wstring();
    }
    int requiredSize = MultiByteToWideChar(CP_UTF8, 0, &sourceString[0], static_cast<int>(sourceString.size()), NULL, 0);
    std::wstring wideStringDestination(requiredSize, 0);
    MultiByteToWideChar(CP_UTF8, 0, &sourceString[0], static_cast<int>(sourceString.size()), &wideStringDestination[0], requiredSize);
    return wideStringDestination;
}

static std::string convertWideStringToString(const std::wstring &sourceWideString)
{
    if (sourceWideString.empty())
    {
        return std::string();
    }
    int requiredSize = WideCharToMultiByte(CP_UTF8, 0, &sourceWideString[0], static_cast<int>(sourceWideString.size()), NULL, 0, NULL, NULL);
    std::string stringDestination(requiredSize, 0);
    WideCharToMultiByte(CP_UTF8, 0, &sourceWideString[0], static_cast<int>(sourceWideString.size()), &stringDestination[0], requiredSize, NULL, NULL);
    return stringDestination;
}

static bool isWindowsTestModeEnabled()
{
    HMODULE ntdllModuleHandle = GetModuleHandleA("ntdll.dll");
    if (!ntdllModuleHandle)
    {
        return false;
    }

    using NtQuerySystemInformationFunction = NTSTATUS(WINAPI *)(ULONG, PVOID, ULONG, PULONG);
    auto ntQuerySystemInformationPointer = reinterpret_cast<NtQuerySystemInformationFunction>(GetProcAddress(ntdllModuleHandle, "NtQuerySystemInformation"));
    if (!ntQuerySystemInformationPointer)
    {
        return false;
    }

    struct SystemCodeIntegrityInformationStructure
    {
        ULONG Length;
        ULONG CodeIntegrityOptions;
    } systemCodeIntegrityInformation = {0};

    systemCodeIntegrityInformation.Length = sizeof(systemCodeIntegrityInformation);
    ULONG returnedLength = 0;

    NTSTATUS queryStatus = ntQuerySystemInformationPointer(103, &systemCodeIntegrityInformation, sizeof(systemCodeIntegrityInformation), &returnedLength);
    if (queryStatus >= 0)
    {
        return (systemCodeIntegrityInformation.CodeIntegrityOptions & 0x0002) != 0;
    }
    return false;
}

namespace injector
{

static std::string loadDriver()
{
    if (!isWindowsTestModeEnabled())
    {
        return "driver load failed windows test mode must be enabled to use kernel methods";
    }

    NTSTATUS status = blackbone::Driver().EnsureLoaded();
    if (NT_SUCCESS(status))
    {
        return "";
    }

    if (status == 0xC0000034)
    {
        return "driver load failed sys file not found make sure its next to the exe";
    }
    if (status == 0xC0000022)
    {
        return "driver load failed access denied please run the injector as an administrator";
    }

    return "driver load failed with unknown error code " + std::to_string(status);
}

std::string professionalManualMap(DWORD targetProcessIdentifier, const std::string &dllPath)
{
    blackbone::Process targetProcess;
    NTSTATUS attachmentStatus = targetProcess.Attach(targetProcessIdentifier);
    if (!NT_SUCCESS(attachmentStatus))
    {
        return "failed to attach to process id " + std::to_string(targetProcessIdentifier);
    }

    blackbone::eLoadFlags mappingFlags = static_cast<blackbone::eLoadFlags>(blackbone::ManualImports | blackbone::CreateLdrRef | blackbone::WipeHeader);

    auto mappingResult = targetProcess.mmap().MapImage(convertStringToWideString(dllPath), mappingFlags);
    if (!NT_SUCCESS(mappingResult.status))
    {
        return "manual mapping failed status " + std::to_string(mappingResult.status);
    }

    return "manual mapping successful";
}

std::string standardInjection(DWORD targetProcessIdentifier, const std::string &dllPath)
{
    blackbone::Process targetProcess;
    if (!NT_SUCCESS(targetProcess.Attach(targetProcessIdentifier)))
    {
        return "failed to attach to process id " + std::to_string(targetProcessIdentifier);
    }

    auto injectionResult = targetProcess.modules().Inject(convertStringToWideString(dllPath));
    if (!NT_SUCCESS(injectionResult.status))
    {
        return "standard injection failed status " + std::to_string(injectionResult.status);
    }

    return "standard injection successful";
}

std::string pureILInjection(DWORD targetProcessIdentifier, const std::string &netVersion, const std::string &dllPath,
                            const std::string &methodName, const std::string &arguments)
{
    blackbone::Process targetProcess;
    if (!NT_SUCCESS(targetProcess.Attach(targetProcessIdentifier)))
    {
        return "failed to attach to process id " + std::to_string(targetProcessIdentifier);
    }

    DWORD injectionReturnCode = 0;
    bool executionSuccess = targetProcess.modules().InjectPureIL(
        convertStringToWideString(netVersion),
        convertStringToWideString(dllPath),
        convertStringToWideString(methodName),
        convertStringToWideString(arguments),
        injectionReturnCode
    );

    if (!executionSuccess)
    {
        return "pure il injection failed";
    }

    return "pure il injection successful return code " + std::to_string(injectionReturnCode);
}

std::string kernelStandardInjection(DWORD targetProcessIdentifier, const std::string &dllPath)
{
    std::string driverErrorMessage = loadDriver();
    if (!driverErrorMessage.empty())
    {
        return driverErrorMessage;
    }

    NTSTATUS driverInjectionStatus = blackbone::Driver().InjectDll(
        targetProcessIdentifier,
        convertStringToWideString(dllPath),
        IT_Thread,
        0,
        L"",
        false,
        false,
        true
    );

    if (!NT_SUCCESS(driverInjectionStatus))
    {
        return "kernel injection failed status " + std::to_string(driverInjectionStatus);
    }

    return "kernel injection successful";
}

std::string kernelManualMap(DWORD targetProcessIdentifier, const std::string &dllPath)
{
    std::string driverErrorMessage = loadDriver();
    if (!driverErrorMessage.empty())
    {
        return driverErrorMessage;
    }

    KMmapFlags kernelMappingFlags = static_cast<KMmapFlags>(KManualImports | KWipeHeader | KHideVAD);
    NTSTATUS kernelMappingStatus = blackbone::Driver().MmapDll(
        targetProcessIdentifier,
        convertStringToWideString(dllPath),
        kernelMappingFlags
    );

    if (!NT_SUCCESS(kernelMappingStatus))
    {
        return "kernel manual mapping failed status " + std::to_string(kernelMappingStatus);
    }

    return "kernel manual mapping successful";
}

std::vector<procInfo> getProcs()
{
    std::vector<procInfo> processList;
    auto enumerationResult = blackbone::Process::EnumByNameOrPID(0, L"");

    if (enumerationResult)
    {
        for (const auto &processEntry : enumerationResult.result())
        {
            procInfo information;
            information.pid = processEntry.pid;
            information.name = convertWideStringToString(processEntry.imageName);

            blackbone::Process temporaryProcess;
            if (NT_SUCCESS(temporaryProcess.Attach(processEntry.pid, PROCESS_QUERY_LIMITED_INFORMATION)))
            {
                information.arch = temporaryProcess.core().isWow64() ? "x86" : "x64";
                temporaryProcess.Detach();
            }
            else
            {
                information.arch = "n/a";
            }
            processList.push_back(information);
        }
    }
    return processList;
}

std::string injectApc(DWORD targetProcessIdentifier, const std::string &dllPath)
{
    blackbone::Process targetProcess;
    if (!NT_SUCCESS(targetProcess.Attach(targetProcessIdentifier)))
    {
        return "failed to attach to process id " + std::to_string(targetProcessIdentifier);
    }

    std::wstring wideDllPath = convertStringToWideString(dllPath);

    auto exportData = targetProcess.modules().GetExport(L"kernel32.dll", "LoadLibraryW");
    if (!NT_SUCCESS(exportData.status))
    {
        return "failed to find LoadLibraryW export in kernel32";
    }

    size_t requiredPathSizeBytes = (wideDllPath.length() + 1) * sizeof(wchar_t);
    auto allocatedMemory = targetProcess.memory().Allocate(requiredPathSizeBytes, PAGE_READWRITE, 0, false);

    if (!NT_SUCCESS(allocatedMemory.status))
    {
        return "memory allocation failed in target process";
    }

    allocatedMemory->Write(0, requiredPathSizeBytes, wideDllPath.c_str());

    auto processThreads = targetProcess.threads().getAll();
    if (processThreads.empty())
    {
        return "process has no active threads";
    }

    int queuedApcCount = 0;

    for (auto &individualThread : processThreads)
    {
        if (individualThread->Suspended())
        {
            continue;
        }

        NTSTATUS queueStatus = targetProcess.core().native()->QueueApcT(
            individualThread->handle(),
            exportData->procAddress,
            allocatedMemory->ptr()
        );

        if (NT_SUCCESS(queueStatus))
        {
            queuedApcCount++;
        }
    }

    if (queuedApcCount == 0)
    {
        return "failed to queue apc no suitable active threads found";
    }

    bool successfullyLoadedModule = false;

    for (int retryAttempt = 0; retryAttempt < 20; retryAttempt++)
    {
        Sleep(50);

        if (targetProcess.modules().GetModule(wideDllPath) != nullptr)
        {
            successfullyLoadedModule = true;
            break;
        }
    }

    if (!successfullyLoadedModule)
    {
        return "apc queued but module did not load within timeout alertable state required";
    }

    return "apc injection successful";
}

std::string injectThreadHijack(DWORD targetProcessIdentifier, const std::string &dllPath)
{
    blackbone::Process targetProcess;
    if (!NT_SUCCESS(targetProcess.Attach(targetProcessIdentifier)))
    {
        return "failed to attach to process id " + std::to_string(targetProcessIdentifier);
    }

    std::wstring wideDllPath = convertStringToWideString(dllPath);
    blackbone::ThreadPtr targetThreadPointer = targetProcess.threads().getMain();

    if (!targetThreadPointer || targetThreadPointer->Suspended())
    {
        auto allThreads = targetProcess.threads().getAll();
        for (auto &individualThread : allThreads)
        {
            if (!individualThread->Suspended())
            {
                targetThreadPointer = individualThread;
                break;
            }
        }
    }

    if (!targetThreadPointer)
    {
        return "failed to locate a suitable thread to hijack";
    }

    try
    {
        auto hijackResult = targetProcess.modules().Inject(wideDllPath, targetThreadPointer);

        if (!NT_SUCCESS(hijackResult.status))
        {
            return "hijack injection failed status " + std::to_string(hijackResult.status);
        }
    }
    catch (const std::exception &caughtException)
    {
        return std::string("injector exception occurred ") + caughtException.what();
    }
    catch (...)
    {
        return "injector caught unknown memory exception";
    }

    return "thread hijack injection successful";
}

std::string injectBlackBone(DWORD targetProcessIdentifier, const std::string &dllPath, bool erasePeHeaders, bool hideModuleMemory)
{
    blackbone::Process targetProcess;
    if (!NT_SUCCESS(targetProcess.Attach(targetProcessIdentifier)))
    {
        return "blackbone failed to attach to process id " + std::to_string(targetProcessIdentifier);
    }

    blackbone::eLoadFlags mappingFlags = blackbone::NoFlags;
    if (!hideModuleMemory)
    {
        mappingFlags |= blackbone::CreateLdrRef;
    }
    if (erasePeHeaders)
    {
        mappingFlags |= blackbone::WipeHeader;
    }

    auto mappingResult = targetProcess.mmap().MapImage(convertStringToWideString(dllPath), mappingFlags);
    targetProcess.Detach();

    if (!mappingResult)
    {
        return "blackbone injection failed status " + std::to_string(mappingResult.status);
    }
    return "blackbone injection successful";
}
}
