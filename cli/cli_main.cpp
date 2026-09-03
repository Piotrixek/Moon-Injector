#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include <BlackBone/Process/Process.h>
#include <BlackBone/Process/ProcessModules.h>

#include "db_handler.h"
#include "debug_engine.h"
#include "injector.h"

using JsonDocument = nlohmann::json;

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

static std::string getExecutableDirectoryPath()
{
    char executablePathBuffer[MAX_PATH] = {0};
    GetModuleFileNameA(NULL, executablePathBuffer, MAX_PATH);
    return std::filesystem::path(executablePathBuffer).parent_path().string();
}

static std::string getDefaultCrashReportsDirectoryPath()
{
    std::filesystem::path reportsPath = std::filesystem::path(getExecutableDirectoryPath()) / "crash_reports";
    return reportsPath.string();
}

static std::string getDatabaseFilePath()
{
    std::filesystem::path executableDirectory = std::filesystem::path(getExecutableDirectoryPath());
    std::filesystem::path primaryDatabasePath = executableDirectory / "dll_list.db";
    if (std::filesystem::exists(primaryDatabasePath))
    {
        return primaryDatabasePath.string();
    }
    return primaryDatabasePath.string();
}

static void handleListProcessesCommand()
{
    try
    {
        std::vector<procInfo> processes = injector::getProcs();
        JsonDocument processArray = JsonDocument::array();
        for (const auto &singleProcess : processes)
        {
            JsonDocument processObject;
            processObject["processIdentifier"] = singleProcess.pid;
            processObject["processName"] = singleProcess.name;
            processObject["architecture"] = singleProcess.arch;
            processArray.push_back(processObject);
        }
        std::cout << processArray.dump(2) << "\n";
    }
    catch (const std::exception &caughtException)
    {
        JsonDocument errorObject;
        errorObject["error"] = caughtException.what();
        std::cout << errorObject.dump(2) << "\n";
    }
}

static void handleGetProcessModulesCommand(int argumentCount, char *argumentVector[])
{
    DWORD targetProcessIdentifier = 0;
    for (int argumentIndex = 2; argumentIndex < argumentCount; ++argumentIndex)
    {
        std::string currentArgument = argumentVector[argumentIndex];
        if (currentArgument == "--pid" && argumentIndex + 1 < argumentCount)
        {
            targetProcessIdentifier = static_cast<DWORD>(std::stoul(argumentVector[++argumentIndex]));
        }
    }
    if (targetProcessIdentifier == 0)
    {
        JsonDocument errorObject;
        errorObject["error"] = "Missing --pid argument";
        std::cout << errorObject.dump(2) << "\n";
        return;
    }
    try
    {
        blackbone::Process targetProcess;
        NTSTATUS attachmentStatus = targetProcess.Attach(targetProcessIdentifier, PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ);
        if (!NT_SUCCESS(attachmentStatus))
        {
            attachmentStatus = targetProcess.Attach(targetProcessIdentifier);
        }
        if (!NT_SUCCESS(attachmentStatus))
        {
            JsonDocument errorObject;
            errorObject["error"] = "Failed to attach to process " + std::to_string(targetProcessIdentifier);
            std::cout << errorObject.dump(2) << "\n";
            return;
        }

        const auto &allModules = targetProcess.modules().GetAllModules();
        JsonDocument modulesArray = JsonDocument::array();
        for (const auto &moduleEntry : allModules)
        {
            const auto &moduleData = moduleEntry.second;
            if (!moduleData)
            {
                continue;
            }
            std::string moduleName = convertWideStringToString(moduleData->name);
            std::string fullPath = convertWideStringToString(moduleData->fullPath);
            std::stringstream hexStream;
            hexStream << "0x" << std::hex << std::uppercase << moduleData->baseAddress;

            JsonDocument singleModuleObject;
            singleModuleObject["moduleName"] = moduleName;
            singleModuleObject["fileName"] = fullPath;
            singleModuleObject["baseAddress"] = hexStream.str();
            singleModuleObject["moduleMemorySize"] = moduleData->size;
            modulesArray.push_back(singleModuleObject);
        }
        std::cout << modulesArray.dump(2) << "\n";
    }
    catch (const std::exception &caughtException)
    {
        JsonDocument errorObject;
        errorObject["error"] = caughtException.what();
        std::cout << errorObject.dump(2) << "\n";
    }
}

static void handleEjectCommand(int argumentCount, char *argumentVector[])
{
    DWORD targetProcessIdentifier = 0;
    std::string moduleNameOrPath = "";
    for (int argumentIndex = 2; argumentIndex < argumentCount; ++argumentIndex)
    {
        std::string currentArgument = argumentVector[argumentIndex];
        if (currentArgument == "--pid" && argumentIndex + 1 < argumentCount)
        {
            targetProcessIdentifier = static_cast<DWORD>(std::stoul(argumentVector[++argumentIndex]));
        }
        else if (currentArgument == "--module" && argumentIndex + 1 < argumentCount)
        {
            moduleNameOrPath = argumentVector[++argumentIndex];
        }
    }
    if (targetProcessIdentifier == 0 || moduleNameOrPath.empty())
    {
        JsonDocument errorObject;
        errorObject["error"] = "Missing --pid or --module argument";
        std::cout << errorObject.dump(2) << "\n";
        return;
    }
    try
    {
        blackbone::Process targetProcess;
        NTSTATUS attachmentStatus = targetProcess.Attach(targetProcessIdentifier);
        if (!NT_SUCCESS(attachmentStatus))
        {
            JsonDocument errorObject;
            errorObject["success"] = false;
            errorObject["message"] = "Failed to attach to process " + std::to_string(targetProcessIdentifier);
            std::cout << errorObject.dump(2) << "\n";
            return;
        }

        std::wstring wideModuleName = convertStringToWideString(moduleNameOrPath);
        auto foundModule = targetProcess.modules().GetModule(wideModuleName);
        if (!foundModule)
        {
            JsonDocument errorObject;
            errorObject["success"] = false;
            errorObject["message"] = "Module not found in target process";
            std::cout << errorObject.dump(2) << "\n";
            return;
        }

        NTSTATUS unloadStatus = targetProcess.modules().Unload(foundModule);
        bool success = NT_SUCCESS(unloadStatus);

        JsonDocument resultObject;
        resultObject["success"] = success;
        resultObject["message"] = success ? "Module unloaded successfully" : "Unload failed with status " + std::to_string(unloadStatus);
        resultObject["processIdentifier"] = targetProcessIdentifier;
        resultObject["moduleName"] = moduleNameOrPath;
        std::cout << resultObject.dump(2) << "\n";
    }
    catch (const std::exception &caughtException)
    {
        JsonDocument errorObject;
        errorObject["success"] = false;
        errorObject["message"] = caughtException.what();
        std::cout << errorObject.dump(2) << "\n";
    }
}

static void handleGetMethodsCommand()
{
    JsonDocument methodsArray = JsonDocument::array({
        {
            {"methodIdentifier", "standard"},
            {"methodName", "Standard Injection (LoadLibraryW)"},
            {"description", "Classic injection method using CreateRemoteThread and LoadLibraryW."},
            {"stealthLevel", "Low"},
            {"requiresKernelDriver", false}
        },
        {
            {"methodIdentifier", "manual_map"},
            {"methodName", "BlackBone Professional Manual Mapping"},
            {"description", "Maps PE headers and sections manually into target process memory without registering with Windows loader."},
            {"stealthLevel", "High"},
            {"requiresKernelDriver", false}
        },
        {
            {"methodIdentifier", "apc"},
            {"methodName", "QueueUserAPC Injection"},
            {"description", "Queues an asynchronous procedure call to target process thread to invoke LoadLibrary."},
            {"stealthLevel", "Medium"},
            {"requiresKernelDriver", false}
        },
        {
            {"methodIdentifier", "thread_hijack"},
            {"methodName", "Thread Context Hijacking"},
            {"description", "Suspends target thread redirects instruction pointer and restores context."},
            {"stealthLevel", "High"},
            {"requiresKernelDriver", false}
        },
        {
            {"methodIdentifier", "blackbone"},
            {"methodName", "BlackBone Custom Map"},
            {"description", "Configurable BlackBone mapping with options to erase PE headers and hide module."},
            {"stealthLevel", "High"},
            {"requiresKernelDriver", false}
        },
        {
            {"methodIdentifier", "pure_il"},
            {"methodName", ".NET Pure Intermediate Language Injection"},
            {"description", "Injects and executes managed .NET assembly methods within target CLR environment."},
            {"stealthLevel", "Medium"},
            {"requiresKernelDriver", false}
        },
        {
            {"methodIdentifier", "kernel_standard"},
            {"methodName", "Kernel Driver Standard Injection"},
            {"description", "Uses BlackBone kernel driver to inject from ring 0 kernel space."},
            {"stealthLevel", "Very High"},
            {"requiresKernelDriver", true}
        },
        {
            {"methodIdentifier", "kernel_manual_map"},
            {"methodName", "Kernel Driver Manual Mapping"},
            {"description", "Uses BlackBone kernel driver to manually map from ring 0 and hides virtual address descriptors."},
            {"stealthLevel", "Maximum"},
            {"requiresKernelDriver", true}
        }
    });

    std::cout << methodsArray.dump(2) << "\n";
}

static void handleCheckDebugInfoCommand(int argumentCount, char *argumentVector[])
{
    std::string dynamicLinkLibraryPath = "";
    for (int argumentIndex = 2; argumentIndex < argumentCount; ++argumentIndex)
    {
        std::string currentArgument = argumentVector[argumentIndex];
        if (currentArgument == "--dll" && argumentIndex + 1 < argumentCount)
        {
            dynamicLinkLibraryPath = argumentVector[++argumentIndex];
        }
    }

    if (dynamicLinkLibraryPath.empty())
    {
        JsonDocument errorObject;
        errorObject["error"] = "Missing --dll argument";
        std::cout << errorObject.dump(2) << "\n";
        return;
    }

    DebugInformationValidationResult validationResult = validateDynamicLinkLibraryDebugInfo(dynamicLinkLibraryPath);
    JsonDocument resultObject;
    resultObject["isDebugBuild"] = validationResult.isDebugBuild;
    resultObject["hasPdbFile"] = validationResult.hasPdbFile;
    resultObject["pdbFilePath"] = validationResult.pdbFilePath;
    resultObject["errorMessage"] = validationResult.errorMessage;
    resultObject["dynamicLinkLibraryPath"] = dynamicLinkLibraryPath;

    std::cout << resultObject.dump(2) << "\n";
}

static void handleInjectCommand(int argumentCount, char *argumentVector[])
{
    DWORD targetProcessIdentifier = 0;
    std::string dynamicLinkLibraryPath = "";
    std::string injectionMethod = "standard";
    bool erasePeHeaders = false;
    bool hideModuleMemory = false;
    bool enableDebugging = false;
    std::string crashReportsDirectory = getDefaultCrashReportsDirectoryPath();
    std::string dotNetVersion = "v4.0.30319";
    std::string dotNetMethodName = "";
    std::string dotNetArguments = "";

    for (int argumentIndex = 2; argumentIndex < argumentCount; ++argumentIndex)
    {
        std::string currentArgument = argumentVector[argumentIndex];
        if (currentArgument == "--pid" && argumentIndex + 1 < argumentCount)
        {
            targetProcessIdentifier = static_cast<DWORD>(std::stoul(argumentVector[++argumentIndex]));
        }
        else if (currentArgument == "--dll" && argumentIndex + 1 < argumentCount)
        {
            dynamicLinkLibraryPath = argumentVector[++argumentIndex];
        }
        else if (currentArgument == "--method" && argumentIndex + 1 < argumentCount)
        {
            injectionMethod = argumentVector[++argumentIndex];
        }
        else if (currentArgument == "--erase-pe")
        {
            erasePeHeaders = true;
        }
        else if (currentArgument == "--hide-module")
        {
            hideModuleMemory = true;
        }
        else if (currentArgument == "--debug")
        {
            enableDebugging = true;
        }
        else if (currentArgument == "--dump-dir" && argumentIndex + 1 < argumentCount)
        {
            crashReportsDirectory = argumentVector[++argumentIndex];
        }
        else if (currentArgument == "--net-version" && argumentIndex + 1 < argumentCount)
        {
            dotNetVersion = argumentVector[++argumentIndex];
        }
        else if (currentArgument == "--method-name" && argumentIndex + 1 < argumentCount)
        {
            dotNetMethodName = argumentVector[++argumentIndex];
        }
        else if (currentArgument == "--args" && argumentIndex + 1 < argumentCount)
        {
            dotNetArguments = argumentVector[++argumentIndex];
        }
    }

    if (targetProcessIdentifier == 0 || dynamicLinkLibraryPath.empty())
    {
        JsonDocument errorObject;
        errorObject["success"] = false;
        errorObject["message"] = "Missing required arguments --pid or --dll";
        std::cout << errorObject.dump(2) << "\n";
        return;
    }

    std::string resolvedPdbPath = "";
    if (enableDebugging)
    {
        DebugInformationValidationResult validationResult = validateDynamicLinkLibraryDebugInfo(dynamicLinkLibraryPath);
        if (!validationResult.isDebugBuild || !validationResult.hasPdbFile)
        {
            JsonDocument rejectionObject;
            rejectionObject["success"] = false;
            rejectionObject["debugValidationFailed"] = true;
            rejectionObject["message"] = "Debug injection rejected: " + validationResult.errorMessage;
            rejectionObject["dynamicLinkLibraryPath"] = dynamicLinkLibraryPath;
            std::cout << rejectionObject.dump(2) << "\n";
            return;
        }
        resolvedPdbPath = validationResult.pdbFilePath;
    }

    std::string injectionStatusMessage = "";
    bool operationSuccessful = false;

    try
    {
        if (injectionMethod == "standard")
        {
            injectionStatusMessage = injector::standardInjection(targetProcessIdentifier, dynamicLinkLibraryPath);
        }
        else if (injectionMethod == "manual_map")
        {
            injectionStatusMessage = injector::professionalManualMap(targetProcessIdentifier, dynamicLinkLibraryPath);
        }
        else if (injectionMethod == "apc")
        {
            injectionStatusMessage = injector::injectApc(targetProcessIdentifier, dynamicLinkLibraryPath);
        }
        else if (injectionMethod == "thread_hijack")
        {
            injectionStatusMessage = injector::injectThreadHijack(targetProcessIdentifier, dynamicLinkLibraryPath);
        }
        else if (injectionMethod == "blackbone")
        {
            injectionStatusMessage = injector::injectBlackBone(targetProcessIdentifier, dynamicLinkLibraryPath, erasePeHeaders, hideModuleMemory);
        }
        else if (injectionMethod == "pure_il")
        {
            injectionStatusMessage = injector::pureILInjection(targetProcessIdentifier, dotNetVersion, dynamicLinkLibraryPath, dotNetMethodName, dotNetArguments);
        }
        else if (injectionMethod == "kernel_standard")
        {
            injectionStatusMessage = injector::kernelStandardInjection(targetProcessIdentifier, dynamicLinkLibraryPath);
        }
        else if (injectionMethod == "kernel_manual_map")
        {
            injectionStatusMessage = injector::kernelManualMap(targetProcessIdentifier, dynamicLinkLibraryPath);
        }
        else
        {
            injectionStatusMessage = "Unsupported injection method: " + injectionMethod;
        }

        operationSuccessful = (injectionStatusMessage.find("successful") != std::string::npos ||
                               injectionStatusMessage.find("success") != std::string::npos);
    }
    catch (const std::exception &caughtException)
    {
        injectionStatusMessage = std::string("Exception occurred: ") + caughtException.what();
        operationSuccessful = false;
    }

    bool watcherSpawned = false;
    if (operationSuccessful && enableDebugging)
    {
        char executablePathBuffer[MAX_PATH] = {0};
        GetModuleFileNameA(NULL, executablePathBuffer, MAX_PATH);

        std::string watcherCommandLine = std::string("\"") + executablePathBuffer + "\" debug-watch --pid " +
                                         std::to_string(targetProcessIdentifier) + " --dll \"" + dynamicLinkLibraryPath +
                                         "\" --dump-dir \"" + crashReportsDirectory + "\"";

        STARTUPINFOA startupInfo = {0};
        startupInfo.cb = sizeof(STARTUPINFOA);
        PROCESS_INFORMATION processInformation = {0};

        std::vector<char> commandLineBuffer(watcherCommandLine.begin(), watcherCommandLine.end());
        commandLineBuffer.push_back('\0');

        BOOL processCreationSuccess = CreateProcessA(NULL, commandLineBuffer.data(), NULL, NULL, FALSE,
                                                     CREATE_NO_WINDOW | DETACHED_PROCESS, NULL, NULL, &startupInfo, &processInformation);

        if (processCreationSuccess)
        {
            CloseHandle(processInformation.hThread);
            CloseHandle(processInformation.hProcess);
            watcherSpawned = true;
        }
    }

    JsonDocument resultObject;
    resultObject["success"] = operationSuccessful;
    resultObject["message"] = injectionStatusMessage;
    resultObject["processIdentifier"] = targetProcessIdentifier;
    resultObject["dynamicLinkLibraryPath"] = dynamicLinkLibraryPath;
    resultObject["injectionMethod"] = injectionMethod;
    resultObject["debugModeEnabled"] = enableDebugging;
    if (enableDebugging)
    {
        resultObject["pdbFilePath"] = resolvedPdbPath;
        resultObject["crashReportsDirectory"] = crashReportsDirectory;
        resultObject["watcherSpawned"] = watcherSpawned;
    }

    std::cout << resultObject.dump(2) << "\n";
}

static void handleDebugWatchCommand(int argumentCount, char *argumentVector[])
{
    DWORD targetProcessIdentifier = 0;
    std::string dynamicLinkLibraryPath = "";
    std::string crashReportsDirectory = getDefaultCrashReportsDirectoryPath();

    for (int argumentIndex = 2; argumentIndex < argumentCount; ++argumentIndex)
    {
        std::string currentArgument = argumentVector[argumentIndex];
        if (currentArgument == "--pid" && argumentIndex + 1 < argumentCount)
        {
            targetProcessIdentifier = static_cast<DWORD>(std::stoul(argumentVector[++argumentIndex]));
        }
        else if (currentArgument == "--dll" && argumentIndex + 1 < argumentCount)
        {
            dynamicLinkLibraryPath = argumentVector[++argumentIndex];
        }
        else if (currentArgument == "--dump-dir" && argumentIndex + 1 < argumentCount)
        {
            crashReportsDirectory = argumentVector[++argumentIndex];
        }
    }

    if (targetProcessIdentifier == 0 || dynamicLinkLibraryPath.empty())
    {
        JsonDocument errorObject;
        errorObject["error"] = "Missing --pid or --dll argument for debug-watch";
        std::cout << errorObject.dump(2) << "\n";
        return;
    }

    CrashReportData crashReport;
    bool crashCaptured = runCrashDebuggerSession(targetProcessIdentifier, dynamicLinkLibraryPath, crashReportsDirectory, crashReport);

    JsonDocument statusObject;
    statusObject["crashCaptured"] = crashCaptured;
    statusObject["targetProcessIdentifier"] = targetProcessIdentifier;
    statusObject["dynamicLinkLibraryPath"] = dynamicLinkLibraryPath;
    if (crashCaptured)
    {
        statusObject["exceptionCode"] = crashReport.exceptionCode;
        statusObject["exceptionDescription"] = crashReport.exceptionDescription;
        statusObject["minidumpFilePath"] = crashReport.minidumpFilePath;
        statusObject["reportJsonFilePath"] = crashReport.reportJsonFilePath;
        statusObject["reportLogFilePath"] = crashReport.reportLogFilePath;
    }
    std::cout << statusObject.dump(2) << "\n";
}

static void handleGetCrashReportsCommand(int argumentCount, char *argumentVector[])
{
    std::string crashReportsDirectory = getDefaultCrashReportsDirectoryPath();
    for (int argumentIndex = 2; argumentIndex < argumentCount; ++argumentIndex)
    {
        std::string currentArgument = argumentVector[argumentIndex];
        if (currentArgument == "--dir" && argumentIndex + 1 < argumentCount)
        {
            crashReportsDirectory = argumentVector[++argumentIndex];
        }
    }

    std::vector<std::string> reportFiles = listAvailableCrashReports(crashReportsDirectory);
    JsonDocument reportsArray = JsonDocument::array();

    for (const auto &singleFilePath : reportFiles)
    {
        try
        {
            std::ifstream fileStream(singleFilePath);
            if (fileStream.is_open())
            {
                JsonDocument singleReportJson;
                fileStream >> singleReportJson;
                reportsArray.push_back(singleReportJson);
            }
        }
        catch (...)
        {
            JsonDocument fallbackItem;
            fallbackItem["filePath"] = singleFilePath;
            reportsArray.push_back(fallbackItem);
        }
    }

    std::cout << reportsArray.dump(2) << "\n";
}

static void handleGetLatestCrashCommand(int argumentCount, char *argumentVector[])
{
    std::string crashReportsDirectory = getDefaultCrashReportsDirectoryPath();
    DWORD targetProcessIdentifier = 0;

    for (int argumentIndex = 2; argumentIndex < argumentCount; ++argumentIndex)
    {
        std::string currentArgument = argumentVector[argumentIndex];
        if (currentArgument == "--dir" && argumentIndex + 1 < argumentCount)
        {
            crashReportsDirectory = argumentVector[++argumentIndex];
        }
        else if (currentArgument == "--pid" && argumentIndex + 1 < argumentCount)
        {
            targetProcessIdentifier = static_cast<DWORD>(std::stoul(argumentVector[++argumentIndex]));
        }
    }

    std::vector<std::string> reportFiles = listAvailableCrashReports(crashReportsDirectory);
    if (reportFiles.empty())
    {
        JsonDocument emptyObject;
        emptyObject["message"] = "No crash reports found";
        std::cout << emptyObject.dump(2) << "\n";
        return;
    }

    std::sort(reportFiles.begin(), reportFiles.end());
    std::string selectedReportPath = "";

    if (targetProcessIdentifier > 0)
    {
        std::string pidPattern = "_" + std::to_string(targetProcessIdentifier) + "_";
        for (auto reverseIterator = reportFiles.rbegin(); reverseIterator != reportFiles.rend(); ++reverseIterator)
        {
            if (reverseIterator->find(pidPattern) != std::string::npos)
            {
                selectedReportPath = *reverseIterator;
                break;
            }
        }
    }
    else
    {
        selectedReportPath = reportFiles.back();
    }

    if (selectedReportPath.empty())
    {
        JsonDocument emptyObject;
        emptyObject["message"] = "No matching crash report found for target process identifier";
        std::cout << emptyObject.dump(2) << "\n";
        return;
    }

    try
    {
        std::ifstream fileStream(selectedReportPath);
        JsonDocument reportJson;
        fileStream >> reportJson;
        std::cout << reportJson.dump(2) << "\n";
    }
    catch (const std::exception &caughtException)
    {
        JsonDocument errorObject;
        errorObject["error"] = caughtException.what();
        std::cout << errorObject.dump(2) << "\n";
    }
}

static void handleResumeProcessCommand(int argumentCount, char *argumentVector[])
{
    DWORD targetProcessIdentifier = 0;
    for (int argumentIndex = 2; argumentIndex < argumentCount; ++argumentIndex)
    {
        std::string currentArgument = argumentVector[argumentIndex];
        if (currentArgument == "--pid" && argumentIndex + 1 < argumentCount)
        {
            targetProcessIdentifier = static_cast<DWORD>(std::stoul(argumentVector[++argumentIndex]));
        }
    }

    if (targetProcessIdentifier == 0)
    {
        JsonDocument errorObject;
        errorObject["error"] = "Missing --pid argument";
        std::cout << errorObject.dump(2) << "\n";
        return;
    }

    bool success = resumeTargetProcess(targetProcessIdentifier);
    JsonDocument resultObject;
    resultObject["success"] = success;
    resultObject["processIdentifier"] = targetProcessIdentifier;
    resultObject["message"] = success ? "Target process threads resumed" : "Failed to resume target process threads";
    std::cout << resultObject.dump(2) << "\n";
}

static void handleTerminateProcessCommand(int argumentCount, char *argumentVector[])
{
    DWORD targetProcessIdentifier = 0;
    for (int argumentIndex = 2; argumentIndex < argumentCount; ++argumentIndex)
    {
        std::string currentArgument = argumentVector[argumentIndex];
        if (currentArgument == "--pid" && argumentIndex + 1 < argumentCount)
        {
            targetProcessIdentifier = static_cast<DWORD>(std::stoul(argumentVector[++argumentIndex]));
        }
    }

    if (targetProcessIdentifier == 0)
    {
        JsonDocument errorObject;
        errorObject["error"] = "Missing --pid argument";
        std::cout << errorObject.dump(2) << "\n";
        return;
    }

    bool success = terminateTargetProcess(targetProcessIdentifier);
    JsonDocument resultObject;
    resultObject["success"] = success;
    resultObject["processIdentifier"] = targetProcessIdentifier;
    resultObject["message"] = success ? "Target process terminated" : "Failed to terminate target process";
    std::cout << resultObject.dump(2) << "\n";
}

static void handleGetWorkspacesCommand()
{
    try
    {
        DBHandler databaseHandler(getDatabaseFilePath());
        std::vector<Workspace> workspaces = databaseHandler.getWorkspaces();
        JsonDocument workspacesArray = JsonDocument::array();
        for (const auto &singleWorkspace : workspaces)
        {
            JsonDocument workspaceObject;
            workspaceObject["workspaceIdentifier"] = singleWorkspace.id;
            workspaceObject["workspaceName"] = singleWorkspace.name;
            workspacesArray.push_back(workspaceObject);
        }
        std::cout << workspacesArray.dump(2) << "\n";
    }
    catch (const std::exception &caughtException)
    {
        JsonDocument errorObject;
        errorObject["error"] = caughtException.what();
        std::cout << errorObject.dump(2) << "\n";
    }
}

static void handleCreateWorkspaceCommand(int argumentCount, char *argumentVector[])
{
    std::string newWorkspaceName = "";
    for (int argumentIndex = 2; argumentIndex < argumentCount; ++argumentIndex)
    {
        std::string currentArgument = argumentVector[argumentIndex];
        if (currentArgument == "--name" && argumentIndex + 1 < argumentCount)
        {
            newWorkspaceName = argumentVector[++argumentIndex];
        }
    }
    if (newWorkspaceName.empty())
    {
        JsonDocument errorObject;
        errorObject["error"] = "Missing --name argument";
        std::cout << errorObject.dump(2) << "\n";
        return;
    }
    try
    {
        DBHandler databaseHandler(getDatabaseFilePath());
        long long createdIdentifier = databaseHandler.addWorkspace(newWorkspaceName);
        JsonDocument resultObject;
        resultObject["success"] = (createdIdentifier > 0);
        resultObject["workspaceIdentifier"] = createdIdentifier;
        resultObject["workspaceName"] = newWorkspaceName;
        std::cout << resultObject.dump(2) << "\n";
    }
    catch (const std::exception &caughtException)
    {
        JsonDocument errorObject;
        errorObject["error"] = caughtException.what();
        std::cout << errorObject.dump(2) << "\n";
    }
}

static void handleDeleteWorkspaceCommand(int argumentCount, char *argumentVector[])
{
    int targetWorkspaceIdentifier = 0;
    for (int argumentIndex = 2; argumentIndex < argumentCount; ++argumentIndex)
    {
        std::string currentArgument = argumentVector[argumentIndex];
        if (currentArgument == "--id" && argumentIndex + 1 < argumentCount)
        {
            targetWorkspaceIdentifier = std::stoi(argumentVector[++argumentIndex]);
        }
    }
    if (targetWorkspaceIdentifier == 0)
    {
        JsonDocument errorObject;
        errorObject["error"] = "Missing --id argument";
        std::cout << errorObject.dump(2) << "\n";
        return;
    }
    try
    {
        DBHandler databaseHandler(getDatabaseFilePath());
        databaseHandler.deleteWorkspace(targetWorkspaceIdentifier);
        JsonDocument resultObject;
        resultObject["success"] = true;
        resultObject["deletedWorkspaceIdentifier"] = targetWorkspaceIdentifier;
        std::cout << resultObject.dump(2) << "\n";
    }
    catch (const std::exception &caughtException)
    {
        JsonDocument errorObject;
        errorObject["error"] = caughtException.what();
        std::cout << errorObject.dump(2) << "\n";
    }
}

static void handleGetWorkspaceDllsCommand(int argumentCount, char *argumentVector[])
{
    int targetWorkspaceIdentifier = 0;
    for (int argumentIndex = 2; argumentIndex < argumentCount; ++argumentIndex)
    {
        std::string currentArgument = argumentVector[argumentIndex];
        if (currentArgument == "--id" && argumentIndex + 1 < argumentCount)
        {
            targetWorkspaceIdentifier = std::stoi(argumentVector[++argumentIndex]);
        }
    }
    if (targetWorkspaceIdentifier == 0)
    {
        JsonDocument errorObject;
        errorObject["error"] = "Missing --id argument";
        std::cout << errorObject.dump(2) << "\n";
        return;
    }
    try
    {
        DBHandler databaseHandler(getDatabaseFilePath());
        std::vector<DllInfo> dynamicLinkLibraries = databaseHandler.getDlls(targetWorkspaceIdentifier);
        JsonDocument librariesArray = JsonDocument::array();
        for (const auto &singleLibrary : dynamicLinkLibraries)
        {
            JsonDocument libraryObject;
            libraryObject["dynamicLinkLibraryPath"] = singleLibrary.path;
            librariesArray.push_back(libraryObject);
        }
        std::cout << librariesArray.dump(2) << "\n";
    }
    catch (const std::exception &caughtException)
    {
        JsonDocument errorObject;
        errorObject["error"] = caughtException.what();
        std::cout << errorObject.dump(2) << "\n";
    }
}

static void handleSyncWorkspaceDllsCommand(int argumentCount, char *argumentVector[])
{
    int targetWorkspaceIdentifier = 0;
    std::vector<std::string> dynamicLinkLibraryPaths;
    for (int argumentIndex = 2; argumentIndex < argumentCount; ++argumentIndex)
    {
        std::string currentArgument = argumentVector[argumentIndex];
        if (currentArgument == "--id" && argumentIndex + 1 < argumentCount)
        {
            targetWorkspaceIdentifier = std::stoi(argumentVector[++argumentIndex]);
        }
        else if (currentArgument == "--dll" && argumentIndex + 1 < argumentCount)
        {
            dynamicLinkLibraryPaths.push_back(argumentVector[++argumentIndex]);
        }
    }
    if (targetWorkspaceIdentifier == 0)
    {
        JsonDocument errorObject;
        errorObject["error"] = "Missing --id argument";
        std::cout << errorObject.dump(2) << "\n";
        return;
    }
    try
    {
        DBHandler databaseHandler(getDatabaseFilePath());
        databaseHandler.syncDlls(targetWorkspaceIdentifier, dynamicLinkLibraryPaths);
        JsonDocument resultObject;
        resultObject["success"] = true;
        resultObject["workspaceIdentifier"] = targetWorkspaceIdentifier;
        resultObject["synchronizedCount"] = dynamicLinkLibraryPaths.size();
        std::cout << resultObject.dump(2) << "\n";
    }
    catch (const std::exception &caughtException)
    {
        JsonDocument errorObject;
        errorObject["error"] = caughtException.what();
        std::cout << errorObject.dump(2) << "\n";
    }
}

int main(int argumentCount, char *argumentVector[])
{
    if (argumentCount < 2)
    {
        JsonDocument helpObject;
        helpObject["name"] = "MoonCLI";
        helpObject["usage"] = "MoonCLI <command> [options]";
        helpObject["commands"] = JsonDocument::array({
            "list-processes",
            "get-process-modules",
            "inject",
            "eject",
            "get-methods",
            "check-debug-info",
            "debug-watch",
            "get-crash-reports",
            "get-latest-crash",
            "resume-process",
            "terminate-process",
            "get-workspaces",
            "create-workspace",
            "delete-workspace",
            "get-workspace-dlls",
            "sync-workspace-dlls"
        });
        std::cout << helpObject.dump(2) << "\n";
        return 0;
    }

    std::string requestedCommand = argumentVector[1];
    if (requestedCommand == "list-processes")
    {
        handleListProcessesCommand();
    }
    else if (requestedCommand == "get-process-modules")
    {
        handleGetProcessModulesCommand(argumentCount, argumentVector);
    }
    else if (requestedCommand == "inject")
    {
        handleInjectCommand(argumentCount, argumentVector);
    }
    else if (requestedCommand == "eject")
    {
        handleEjectCommand(argumentCount, argumentVector);
    }
    else if (requestedCommand == "get-methods")
    {
        handleGetMethodsCommand();
    }
    else if (requestedCommand == "check-debug-info")
    {
        handleCheckDebugInfoCommand(argumentCount, argumentVector);
    }
    else if (requestedCommand == "debug-watch")
    {
        handleDebugWatchCommand(argumentCount, argumentVector);
    }
    else if (requestedCommand == "get-crash-reports")
    {
        handleGetCrashReportsCommand(argumentCount, argumentVector);
    }
    else if (requestedCommand == "get-latest-crash")
    {
        handleGetLatestCrashCommand(argumentCount, argumentVector);
    }
    else if (requestedCommand == "resume-process")
    {
        handleResumeProcessCommand(argumentCount, argumentVector);
    }
    else if (requestedCommand == "terminate-process")
    {
        handleTerminateProcessCommand(argumentCount, argumentVector);
    }
    else if (requestedCommand == "get-workspaces")
    {
        handleGetWorkspacesCommand();
    }
    else if (requestedCommand == "create-workspace")
    {
        handleCreateWorkspaceCommand(argumentCount, argumentVector);
    }
    else if (requestedCommand == "delete-workspace")
    {
        handleDeleteWorkspaceCommand(argumentCount, argumentVector);
    }
    else if (requestedCommand == "get-workspace-dlls")
    {
        handleGetWorkspaceDllsCommand(argumentCount, argumentVector);
    }
    else if (requestedCommand == "sync-workspace-dlls")
    {
        handleSyncWorkspaceDllsCommand(argumentCount, argumentVector);
    }
    else
    {
        JsonDocument errorObject;
        errorObject["error"] = "Unknown command: " + requestedCommand;
        std::cout << errorObject.dump(2) << "\n";
        return 1;
    }

    return 0;
}
