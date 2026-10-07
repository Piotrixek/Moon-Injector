#include <Windows.h>
#include <gdiplus.h>
#include <objidl.h>
#include <shellapi.h>
#include <tchar.h>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include "dx_setup.h"
#include "ui_style.h"
#include "window_setup.h"

#include "db_handler.h"
#include "font.h"
#include "icons.h"
#include "injector.h"
#include "menu.h"
#include "tinyfiledialogs.h"
#include "ultralight_controller.h"
#include "debug_engine.h"
#include <BlackBone/Process/Process.h>
#include <BlackBone/Process/ProcessModules.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

std::unique_ptr<UltralightController> g_ultralight_controller;
std::unique_ptr<DBHandler> g_db_handler;

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

std::string escapeJsonString(const std::string &inputString)
{
    std::stringstream stringStream;
    for (char character : inputString)
    {
        switch (character)
        {
        case '\\':
            stringStream << "\\\\";
            break;
        case '"':
            stringStream << "\\\"";
            break;
        case '/':
            stringStream << "\\/";
            break;
        case '\b':
            stringStream << "\\b";
            break;
        case '\f':
            stringStream << "\\f";
            break;
        case '\n':
            stringStream << "\\n";
            break;
        case '\r':
            stringStream << "\\r";
            break;
        case '\t':
            stringStream << "\\t";
            break;
        default:
            if (character >= 0 && character < 32)
            {
                stringStream << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                             << static_cast<int>(static_cast<unsigned char>(character));
            }
            else
            {
                stringStream << character;
            }
            break;
        }
    }
    return stringStream.str();
}

struct DynamicLinkLibraryInformation
{
    std::string architectureName;
    bool isValidExecutableImage;
    bool isDebugBuild;
    bool hasPdbFile;
    std::string pdbFilePath;
    uint64_t fileSizeBytes;
};

static DynamicLinkLibraryInformation inspectDynamicLinkLibrary(const std::string &dynamicLinkLibraryPath)
{
    DynamicLinkLibraryInformation informationResult;
    informationResult.architectureName = "unknown";
    informationResult.isValidExecutableImage = false;
    informationResult.isDebugBuild = false;
    informationResult.hasPdbFile = false;
    informationResult.pdbFilePath = "";
    informationResult.fileSizeBytes = 0;

    std::ifstream fileStream(dynamicLinkLibraryPath, std::ios::binary | std::ios::ate);
    if (!fileStream.is_open())
    {
        return informationResult;
    }

    std::streamsize totalFileSize = fileStream.tellg();
    if (totalFileSize < static_cast<std::streamsize>(sizeof(IMAGE_DOS_HEADER)))
    {
        return informationResult;
    }
    informationResult.fileSizeBytes = static_cast<uint64_t>(totalFileSize);

    fileStream.seekg(0, std::ios::beg);
    IMAGE_DOS_HEADER dosHeader = {0};
    fileStream.read(reinterpret_cast<char *>(&dosHeader), sizeof(IMAGE_DOS_HEADER));
    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
    {
        return informationResult;
    }

    if (dosHeader.e_lfanew <= 0 || dosHeader.e_lfanew + static_cast<std::streamoff>(sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER)) > totalFileSize)
    {
        return informationResult;
    }

    fileStream.seekg(dosHeader.e_lfanew, std::ios::beg);
    DWORD ntSignature = 0;
    fileStream.read(reinterpret_cast<char *>(&ntSignature), sizeof(DWORD));
    if (ntSignature != IMAGE_NT_SIGNATURE)
    {
        return informationResult;
    }

    IMAGE_FILE_HEADER fileHeader = {0};
    fileStream.read(reinterpret_cast<char *>(&fileHeader), sizeof(IMAGE_FILE_HEADER));

    if (fileHeader.Machine == IMAGE_FILE_MACHINE_I386)
    {
        informationResult.architectureName = "x86";
        informationResult.isValidExecutableImage = true;
    }
    else if (fileHeader.Machine == IMAGE_FILE_MACHINE_AMD64)
    {
        informationResult.architectureName = "x64";
        informationResult.isValidExecutableImage = true;
    }
    else if (fileHeader.Machine == IMAGE_FILE_MACHINE_ARM64)
    {
        informationResult.architectureName = "arm64";
        informationResult.isValidExecutableImage = true;
    }
    else
    {
        informationResult.architectureName = "other";
        informationResult.isValidExecutableImage = true;
    }

    fileStream.close();

    try
    {
        DebugInformationValidationResult debugValidationResult = validateDynamicLinkLibraryDebugInfo(dynamicLinkLibraryPath);
        informationResult.isDebugBuild = debugValidationResult.isDebugBuild;
        informationResult.hasPdbFile = debugValidationResult.hasPdbFile;
        informationResult.pdbFilePath = debugValidationResult.pdbFilePath;
    }
    catch (...)
    {
    }

    return informationResult;
}

struct ProcessModuleEntry
{
    std::string moduleName;
    std::string fullFilePath;
    std::string baseAddressHexadecimal;
    uint32_t moduleMemorySizeBytes;
};

static std::vector<ProcessModuleEntry> enumerateProcessModules(DWORD targetProcessIdentifier)
{
    std::vector<ProcessModuleEntry> moduleCollection;
    if (targetProcessIdentifier == 0)
    {
        return moduleCollection;
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
            return moduleCollection;
        }

        const auto &allModules = targetProcess.modules().GetAllModules();
        for (const auto &modulePair : allModules)
        {
            const auto &moduleData = modulePair.second;
            if (!moduleData)
            {
                continue;
            }

            ProcessModuleEntry moduleEntry;
            moduleEntry.moduleName = convertWideStringToString(moduleData->name);
            moduleEntry.fullFilePath = convertWideStringToString(moduleData->fullPath);
            std::stringstream addressStream;
            addressStream << "0x" << std::hex << std::uppercase << moduleData->baseAddress;
            moduleEntry.baseAddressHexadecimal = addressStream.str();
            moduleEntry.moduleMemorySizeBytes = moduleData->size;

            moduleCollection.push_back(moduleEntry);
        }
        targetProcess.Detach();
    }
    catch (...)
    {
    }

    return moduleCollection;
}

static bool ejectProcessModule(DWORD targetProcessIdentifier, const std::string &moduleNameOrPath)
{
    if (targetProcessIdentifier == 0 || moduleNameOrPath.empty())
    {
        return false;
    }

    try
    {
        blackbone::Process targetProcess;
        NTSTATUS attachmentStatus = targetProcess.Attach(targetProcessIdentifier);
        if (!NT_SUCCESS(attachmentStatus))
        {
            return false;
        }

        std::wstring wideModuleName = convertStringToWideString(moduleNameOrPath);
        auto foundModule = targetProcess.modules().GetModule(wideModuleName);
        if (!foundModule)
        {
            targetProcess.Detach();
            return false;
        }

        NTSTATUS unloadStatus = targetProcess.modules().Unload(foundModule);
        targetProcess.Detach();
        return NT_SUCCESS(unloadStatus);
    }
    catch (...)
    {
        return false;
    }
}

static bool terminateTargetProcess(DWORD targetProcessIdentifier)
{
    if (targetProcessIdentifier == 0)
    {
        return false;
    }

    HANDLE processHandle = OpenProcess(PROCESS_TERMINATE, FALSE, targetProcessIdentifier);
    if (!processHandle)
    {
        return false;
    }

    BOOL terminationResult = TerminateProcess(processHandle, 0);
    CloseHandle(processHandle);
    return terminationResult != FALSE;
}

static bool suspendTargetProcess(DWORD targetProcessIdentifier)
{
    if (targetProcessIdentifier == 0)
    {
        return false;
    }

    try
    {
        blackbone::Process targetProcess;
        if (!NT_SUCCESS(targetProcess.Attach(targetProcessIdentifier)))
        {
            return false;
        }
        NTSTATUS suspendStatus = targetProcess.Suspend();
        targetProcess.Detach();
        return NT_SUCCESS(suspendStatus);
    }
    catch (...)
    {
        return false;
    }
}

static bool resumeTargetProcess(DWORD targetProcessIdentifier)
{
    if (targetProcessIdentifier == 0)
    {
        return false;
    }

    try
    {
        blackbone::Process targetProcess;
        if (!NT_SUCCESS(targetProcess.Attach(targetProcessIdentifier)))
        {
            return false;
        }
        NTSTATUS resumeStatus = targetProcess.Resume();
        targetProcess.Detach();
        return NT_SUCCESS(resumeStatus);
    }
    catch (...)
    {
        return false;
    }
}

static bool setClipboardTextData(HWND windowHandle, const std::string &clipboardText)
{
    if (!OpenClipboard(windowHandle))
    {
        return false;
    }

    EmptyClipboard();
    HGLOBAL globalMemoryHandle = GlobalAlloc(GMEM_MOVEABLE, clipboardText.size() + 1);
    if (!globalMemoryHandle)
    {
        CloseClipboard();
        return false;
    }

    char *globalMemoryPointer = reinterpret_cast<char *>(GlobalLock(globalMemoryHandle));
    if (!globalMemoryPointer)
    {
        GlobalFree(globalMemoryHandle);
        CloseClipboard();
        return false;
    }

    memcpy(globalMemoryPointer, clipboardText.c_str(), clipboardText.size() + 1);
    GlobalUnlock(globalMemoryHandle);
    SetClipboardData(CF_TEXT, globalMemoryHandle);
    CloseClipboard();
    return true;
}

static std::string getCrashReportsFolderPath()
{
    char executablePathBuffer[MAX_PATH] = {0};
    GetModuleFileNameA(NULL, executablePathBuffer, MAX_PATH);
    std::filesystem::path executableDirectory = std::filesystem::path(executablePathBuffer).parent_path();
    std::filesystem::path reportsDirectory = executableDirectory / "crash_reports";
    if (!std::filesystem::exists(reportsDirectory))
    {
        std::filesystem::create_directories(reportsDirectory);
    }
    return reportsDirectory.string();
}

static std::vector<std::string> getCrashReportsList()
{
    try
    {
        return listAvailableCrashReports(getCrashReportsFolderPath());
    }
    catch (...)
    {
        return {};
    }
}

static std::string readCrashReportContent(const std::string &reportFileName)
{
    try
    {
        std::filesystem::path reportsDirectory = std::filesystem::path(getCrashReportsFolderPath());
        std::filesystem::path fullReportPath = reportsDirectory / reportFileName;
        if (!std::filesystem::exists(fullReportPath))
        {
            return "report file not found";
        }

        std::ifstream fileStream(fullReportPath);
        if (!fileStream.is_open())
        {
            return "unable to open report file";
        }

        std::stringstream stringStream;
        stringStream << fileStream.rdbuf();
        return stringStream.str();
    }
    catch (...)
    {
        return "error reading report file";
    }
}

int main(int, char **)
{
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::filesystem::path dbFilePath = std::filesystem::path(exePath).parent_path() / "dll_list.db";

    try
    {
        g_db_handler = std::make_unique<DBHandler>(dbFilePath.string());
    }
    catch (const std::exception &e)
    {
        MessageBoxA(NULL, e.what(), "database error", MB_OK | MB_ICONERROR);
        return 1;
    }

    HINSTANCE hInstance = GetModuleHandle(NULL);
    const TCHAR *className = _T("ImGuiAppClass");
    HWND hwnd = SetupWindow(hInstance, className);
    if (!hwnd)
    {
        return 1;
    }

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::DestroyWindow(hwnd);
        CleanupWindow(hInstance, className);
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    ImFontConfig main_font_config;
    main_font_config.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryTTF((void *)my_Font, sizeof(my_Font), 16.0f, &main_font_config);

    static const ImWchar icons_ranges[] = {0xf000, 0xf3ff, 0};
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.OversampleH = 3;
    icons_config.OversampleV = 3;
    icons_config.EllipsisChar = 0xf141;
    io.Fonts->AddFontFromMemoryCompressedTTF(font_awesome_data, font_awesome_size, 18.5f, &icons_config, icons_ranges);

    ApplyCommandMenuStyle();

    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 0.0f;
    }

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(GetDevice(), GetImmediateContext());

    try
    {
        g_ultralight_controller = std::make_unique<UltralightController>(GetDevice(), GetImmediateContext());
    }
    catch (const std::exception &e)
    {
        std::string error_msg = "an exception occurred during ultralight initialization\n\n";
        error_msg += e.what();
        error_msg += "\n\nthis usually means the resources folder is missing from the executables directory";
        MessageBoxA(NULL, error_msg.c_str(), "ultralight initialization failed", MB_OK | MB_ICONERROR);
        return 1;
    }

    g_ultralight_controller->AddCallback(
        "native_getWorkspaces", JSCallbackFuncWithRet([](const ultralight::JSObject &obj,
                                                         const ultralight::JSArgs &args) -> ultralight::JSValue {
            auto workspaces = g_db_handler->getWorkspaces();
            std::stringstream ss;
            ss << "[";
            for (size_t i = 0; i < workspaces.size(); ++i)
            {
                ss << "{\"id\":" << workspaces[i].id << ", \"name\":\"" << escapeJsonString(workspaces[i].name)
                   << "\"}";
                if (i < workspaces.size() - 1)
                    ss << ",";
            }
            ss << "]";
            return ultralight::JSValue(ss.str().c_str());
        }));

    g_ultralight_controller->AddCallback(
        "native_addWorkspace", JSCallbackFuncWithRet([](const ultralight::JSObject &obj,
                                                         const ultralight::JSArgs &args) -> ultralight::JSValue {
            if (args.size() == 1 && args[0].IsString())
            {
                std::string name = ultralight::String(args[0].ToString()).utf8().data();
                long long newId = g_db_handler->addWorkspace(name);
                return ultralight::JSValue(newId);
            }
            return ultralight::JSValue(0);
        }));

    g_ultralight_controller->AddCallback(
        "native_renameWorkspace", JSCallbackFunc([](const ultralight::JSObject &obj, const ultralight::JSArgs &args) {
            if (args.size() == 2 && args[0].IsNumber() && args[1].IsString())
            {
                int id = (int)args[0].ToNumber();
                std::string name = ultralight::String(args[1].ToString()).utf8().data();
                g_db_handler->renameWorkspace(id, name);
            }
        }));

    g_ultralight_controller->AddCallback(
        "native_deleteWorkspace", JSCallbackFunc([](const ultralight::JSObject &obj, const ultralight::JSArgs &args) {
            if (args.size() == 1 && args[0].IsNumber())
            {
                int id = (int)args[0].ToNumber();
                g_db_handler->deleteWorkspace(id);
            }
        }));

    g_ultralight_controller->AddCallback(
        "native_messageBoxYesNo", JSCallbackFuncWithRet([](const ultralight::JSObject &obj,
                                                           const ultralight::JSArgs &args) -> ultralight::JSValue {
            if (args.size() == 2 && args[0].IsString() && args[1].IsString())
            {
                std::string title = ultralight::String(args[0].ToString()).utf8().data();
                std::string message = ultralight::String(args[1].ToString()).utf8().data();
                int result = tinyfd_messageBox(title.c_str(), message.c_str(), "yesno", "question", 1);
                return ultralight::JSValue(result == 1);
            }
            return ultralight::JSValue(false);
        }));

    g_ultralight_controller->AddCallback(
        "native_getDlls", JSCallbackFuncWithRet([](const ultralight::JSObject &obj,
                                                   const ultralight::JSArgs &args) -> ultralight::JSValue {
            if (args.size() == 1 && args[0].IsNumber())
            {
                int workspaceId = (int)args[0].ToNumber();
                auto dlls = g_db_handler->getDlls(workspaceId);
                std::stringstream ss;
                ss << "[";
                for (size_t i = 0; i < dlls.size(); ++i)
                {
                    ss << "\"" << escapeJsonString(dlls[i].path) << "\"";
                    if (i < dlls.size() - 1)
                        ss << ",";
                }
                ss << "]";
                return ultralight::JSValue(ss.str().c_str());
            }
            return ultralight::JSValue("[]");
        }));

    g_ultralight_controller->AddCallback(
        "native_openDllDialog", JSCallbackFuncWithRet([](const ultralight::JSObject &obj,
                                                         const ultralight::JSArgs &args) -> ultralight::JSValue {
            const char *filterPatterns[1] = {"*.dll"};
            const char *filePath = tinyfd_openFileDialog("select dll", "", 1, filterPatterns, "dll files", 0);
            if (filePath)
                return ultralight::JSValue(filePath);
            return ultralight::JSValue();
        }));

    g_ultralight_controller->AddCallback(
        "native_saveDlls", JSCallbackFunc([](const ultralight::JSObject &obj, const ultralight::JSArgs &args) {
            if (args.size() == 2 && args[0].IsNumber() && args[1].IsString())
            {
                int workspaceId = (int)args[0].ToNumber();
                std::string pathsStr = ultralight::String(args[1].ToString()).utf8().data();
                std::vector<std::string> paths;
                std::string delimiter = "_|_";
                size_t pos = 0;
                std::string token;
                std::string s = pathsStr;
                while ((pos = s.find(delimiter)) != std::string::npos)
                {
                    token = s.substr(0, pos);
                    if (!token.empty())
                        paths.push_back(token);
                    s.erase(0, pos + delimiter.length());
                }
                if (!s.empty())
                    paths.push_back(s);
                g_db_handler->syncDlls(workspaceId, paths);
            }
        }));

    g_ultralight_controller->AddCallback(
        "native_getProcesses", JSCallbackFuncWithRet([](const ultralight::JSObject &obj,
                                                         const ultralight::JSArgs &args) -> ultralight::JSValue {
            auto procs = injector::getProcs();
            std::stringstream ss;
            ss << "[";
            for (size_t i = 0; i < procs.size(); ++i)
            {
                ss << "{\"pid\":" << procs[i].pid << ", \"name\":\"" << escapeJsonString(procs[i].name)
                   << "\", \"arch\":\"" << escapeJsonString(procs[i].arch) << "\"}";
                if (i < procs.size() - 1)
                    ss << ",";
            }
            ss << "]";
            return ultralight::JSValue(ss.str().c_str());
        }));

    g_ultralight_controller->AddCallback(
        "native_quit", JSCallbackFunc([hwnd](const ultralight::JSObject &obj, const ultralight::JSArgs &args) {
            PostMessage(hwnd, WM_QUIT, 0, 0);
        }));

    g_ultralight_controller->AddCallback(
        "native_getProcessModules", JSCallbackFuncWithRet([](const ultralight::JSObject &objectReference,
                                                             const ultralight::JSArgs &functionArguments) -> ultralight::JSValue {
            if (functionArguments.size() < 1 || !functionArguments[0].IsNumber())
            {
                return ultralight::JSValue("[]");
            }
            DWORD processIdentifier = static_cast<DWORD>(functionArguments[0].ToNumber());
            auto moduleEntries = enumerateProcessModules(processIdentifier);
            std::stringstream jsonStream;
            jsonStream << "[";
            for (size_t entryIndex = 0; entryIndex < moduleEntries.size(); ++entryIndex)
            {
                jsonStream << "{\"moduleName\":\"" << escapeJsonString(moduleEntries[entryIndex].moduleName)
                           << "\",\"fullFilePath\":\"" << escapeJsonString(moduleEntries[entryIndex].fullFilePath)
                           << "\",\"baseAddress\":\"" << escapeJsonString(moduleEntries[entryIndex].baseAddressHexadecimal)
                           << "\",\"moduleMemorySize\":" << moduleEntries[entryIndex].moduleMemorySizeBytes << "}";
                if (entryIndex + 1 < moduleEntries.size())
                {
                    jsonStream << ",";
                }
            }
            jsonStream << "]";
            return ultralight::JSValue(jsonStream.str().c_str());
        }));

    g_ultralight_controller->AddCallback(
        "native_ejectModule", JSCallbackFuncWithRet([](const ultralight::JSObject &objectReference,
                                                       const ultralight::JSArgs &functionArguments) -> ultralight::JSValue {
            if (functionArguments.size() < 2 || !functionArguments[0].IsNumber() || !functionArguments[1].IsString())
            {
                return ultralight::JSValue(false);
            }
            DWORD processIdentifier = static_cast<DWORD>(functionArguments[0].ToNumber());
            std::string moduleName = ultralight::String(functionArguments[1].ToString()).utf8().data();
            bool ejectionSuccess = ejectProcessModule(processIdentifier, moduleName);
            return ultralight::JSValue(ejectionSuccess);
        }));

    g_ultralight_controller->AddCallback(
        "native_terminateProcess", JSCallbackFuncWithRet([](const ultralight::JSObject &objectReference,
                                                           const ultralight::JSArgs &functionArguments) -> ultralight::JSValue {
            if (functionArguments.size() < 1 || !functionArguments[0].IsNumber())
            {
                return ultralight::JSValue(false);
            }
            DWORD processIdentifier = static_cast<DWORD>(functionArguments[0].ToNumber());
            bool terminationSuccess = terminateTargetProcess(processIdentifier);
            return ultralight::JSValue(terminationSuccess);
        }));

    g_ultralight_controller->AddCallback(
        "native_suspendProcess", JSCallbackFuncWithRet([](const ultralight::JSObject &objectReference,
                                                         const ultralight::JSArgs &functionArguments) -> ultralight::JSValue {
            if (functionArguments.size() < 1 || !functionArguments[0].IsNumber())
            {
                return ultralight::JSValue(false);
            }
            DWORD processIdentifier = static_cast<DWORD>(functionArguments[0].ToNumber());
            bool suspendSuccess = suspendTargetProcess(processIdentifier);
            return ultralight::JSValue(suspendSuccess);
        }));

    g_ultralight_controller->AddCallback(
        "native_resumeProcess", JSCallbackFuncWithRet([](const ultralight::JSObject &objectReference,
                                                        const ultralight::JSArgs &functionArguments) -> ultralight::JSValue {
            if (functionArguments.size() < 1 || !functionArguments[0].IsNumber())
            {
                return ultralight::JSValue(false);
            }
            DWORD processIdentifier = static_cast<DWORD>(functionArguments[0].ToNumber());
            bool resumeSuccess = resumeTargetProcess(processIdentifier);
            return ultralight::JSValue(resumeSuccess);
        }));

    g_ultralight_controller->AddCallback(
        "native_inspectDll", JSCallbackFuncWithRet([](const ultralight::JSObject &objectReference,
                                                     const ultralight::JSArgs &functionArguments) -> ultralight::JSValue {
            if (functionArguments.size() < 1 || !functionArguments[0].IsString())
            {
                return ultralight::JSValue("{}");
            }
            std::string libraryPath = ultralight::String(functionArguments[0].ToString()).utf8().data();
            auto information = inspectDynamicLinkLibrary(libraryPath);
            std::stringstream jsonStream;
            jsonStream << "{"
                       << "\"architecture\":\"" << escapeJsonString(information.architectureName) << "\","
                       << "\"isValidExecutableImage\":" << (information.isValidExecutableImage ? "true" : "false") << ","
                       << "\"isDebugBuild\":" << (information.isDebugBuild ? "true" : "false") << ","
                       << "\"hasPdbFile\":" << (information.hasPdbFile ? "true" : "false") << ","
                       << "\"pdbFilePath\":\"" << escapeJsonString(information.pdbFilePath) << "\","
                       << "\"fileSizeBytes\":" << information.fileSizeBytes
                       << "}";
            return ultralight::JSValue(jsonStream.str().c_str());
        }));

    g_ultralight_controller->AddCallback(
        "native_getCrashReports", JSCallbackFuncWithRet([](const ultralight::JSObject &objectReference,
                                                          const ultralight::JSArgs &functionArguments) -> ultralight::JSValue {
            auto reportFiles = getCrashReportsList();
            std::stringstream jsonStream;
            jsonStream << "[";
            for (size_t fileIndex = 0; fileIndex < reportFiles.size(); ++fileIndex)
            {
                jsonStream << "\"" << escapeJsonString(reportFiles[fileIndex]) << "\"";
                if (fileIndex + 1 < reportFiles.size())
                {
                    jsonStream << ",";
                }
            }
            jsonStream << "]";
            return ultralight::JSValue(jsonStream.str().c_str());
        }));

    g_ultralight_controller->AddCallback(
        "native_readCrashReport", JSCallbackFuncWithRet([](const ultralight::JSObject &objectReference,
                                                          const ultralight::JSArgs &functionArguments) -> ultralight::JSValue {
            if (functionArguments.size() < 1 || !functionArguments[0].IsString())
            {
                return ultralight::JSValue("");
            }
            std::string reportFileName = ultralight::String(functionArguments[0].ToString()).utf8().data();
            std::string reportContent = readCrashReportContent(reportFileName);
            return ultralight::JSValue(reportContent.c_str());
        }));

    g_ultralight_controller->AddCallback(
        "native_openCrashFolder", JSCallbackFunc([](const ultralight::JSObject &objectReference,
                                                   const ultralight::JSArgs &functionArguments) {
            std::string folderPath = getCrashReportsFolderPath();
            ShellExecuteA(NULL, "open", folderPath.c_str(), NULL, NULL, SW_SHOW);
        }));

    g_ultralight_controller->AddCallback(
        "native_copyToClipboard", JSCallbackFuncWithRet([hwnd](const ultralight::JSObject &objectReference,
                                                              const ultralight::JSArgs &functionArguments) -> ultralight::JSValue {
            if (functionArguments.size() < 1 || !functionArguments[0].IsString())
            {
                return ultralight::JSValue(false);
            }
            std::string textToCopy = ultralight::String(functionArguments[0].ToString()).utf8().data();
            bool copySuccess = setClipboardTextData(hwnd, textToCopy);
            return ultralight::JSValue(copySuccess);
        }));

    g_ultralight_controller->AddCallback(
        "native_inject", JSCallbackFuncWithRet([](const ultralight::JSObject &obj,
                                                  const ultralight::JSArgs &args) -> ultralight::JSValue {
            if (args.size() < 3 || !args[0].IsNumber() || !args[1].IsString() || !args[2].IsString())
            {
                return ultralight::JSValue("invalid arguments for injection");
            }

            DWORD pid = (DWORD)args[0].ToNumber();
            std::string dllPath = ultralight::String(args[1].ToString()).utf8().data();
            std::string method = ultralight::String(args[2].ToString()).utf8().data();

            bool erasePE = args.size() > 3 && args[3].IsBoolean() ? args[3].ToBoolean() : false;
            bool hideModule = args.size() > 4 && args[4].IsBoolean() ? args[4].ToBoolean() : false;

            std::string netVersion =
                args.size() > 5 && args[5].IsString() ? ultralight::String(args[5].ToString()).utf8().data() : "";
            std::string ilMethod =
                args.size() > 6 && args[6].IsString() ? ultralight::String(args[6].ToString()).utf8().data() : "";
            std::string ilArgs =
                args.size() > 7 && args[7].IsString() ? ultralight::String(args[7].ToString()).utf8().data() : "";

            std::string result = "";
            try
            {
                if (method == "standard")
                    result = injector::standardInjection(pid, dllPath);
                else if (method == "apc")
                    result = injector::injectApc(pid, dllPath);
                else if (method == "hijack")
                    result = injector::injectThreadHijack(pid, dllPath);
                else if (method == "blackbone")
                    result = injector::injectBlackBone(pid, dllPath, erasePE, hideModule);
                else if (method == "prommap")
                    result = injector::professionalManualMap(pid, dllPath);
                else if (method == "pureil")
                    result = injector::pureILInjection(pid, netVersion, dllPath, ilMethod, ilArgs);
                else if (method == "kstandard")
                    result = injector::kernelStandardInjection(pid, dllPath);
                else if (method == "kmmap")
                    result = injector::kernelManualMap(pid, dllPath);
                else
                    result = "unknown injection method specified";
            }
            catch (const std::exception &caughtException)
            {
                result = std::string("injection exception ") + caughtException.what();
            }
            catch (...)
            {
                result = "unknown injection memory exception";
            }

            return ultralight::JSValue(result.c_str());
        }));

    if (!g_ultralight_controller->IsRendererValid())
    {
        MessageBox(NULL,
                   _T("failed to initialize ultralight renderer\n\nmake sure the resources and dll files from ")
                   _T("the sdk are in the same directory as your exe"),
                   _T("ultralight error"), MB_OK | MB_ICONERROR);
        return 1;
    }

    std::string html_content = R"HTML_PART1(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Moon Injector</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <script src="https://unpkg.com/@phosphor-icons/web"></script>
    <style>
        @import url('https://fonts.googleapis.com/css2?family=Roboto+Mono:wght@400;500;600&family=Inter:wght@400;500;600;700&display=swap');
        body { font-family: 'Inter', sans-serif; background: transparent; overflow: hidden; }
        .font-mono { font-family: 'Roboto Mono', monospace; }
        .main-window { background-color: #09090b; border: 1px solid #27272a; animation: appear 0.3s ease-out; }
        @keyframes appear { from { opacity: 0; transform: scale(0.995); } to { opacity: 1; transform: scale(1); } }
        ::-webkit-scrollbar { width: 6px; height: 6px; }
        ::-webkit-scrollbar-track { background: #121215; }
        ::-webkit-scrollbar-thumb { background: #27272a; border-radius: 3px; }
        ::-webkit-scrollbar-thumb:hover { background: #3f3f46; }
        
        .btn { background-color: #18181b; border: 1px solid #27272a; color: #d4d4d8; transition: all 0.15s ease; display: inline-flex; align-items: center; justify-content: center; gap: 0.375rem; user-select: none; }
        .btn:hover { border-color: #06b6d4; color: #ffffff; background-color: #27272a; }
        .btn-primary { background: linear-gradient(135deg, #0891b2 0%, #06b6d4 100%); border: 1px solid #22d3ee; color: #ffffff; transition: all 0.2s ease; box-shadow: 0 4px 14px 0 rgba(6, 182, 212, 0.25); }
        .btn-primary:hover { background: linear-gradient(135deg, #06b6d4 0%, #22d3ee 100%); box-shadow: 0 6px 20px 0 rgba(6, 182, 212, 0.4); transform: translateY(-1px); }
        .btn-primary:active { transform: translateY(0); }

        .list-item { display: flex; justify-content: space-between; align-items: center; padding: 0.3rem 0.5rem; cursor: pointer; transition: all 0.12s ease-out; border-radius: 4px; }
        .list-item:hover { background-color: #18181b; }
        .list-item.selected { background-color: rgba(6, 182, 212, 0.12); color: #67e8f9; border: 1px solid rgba(6, 182, 212, 0.35); }
        
        .badge-arch-x64 { background-color: rgba(6, 182, 212, 0.15); color: #22d3ee; border: 1px solid rgba(6, 182, 212, 0.3); font-size: 10px; padding: 1px 4px; border-radius: 3px; font-weight: 600; }
        .badge-arch-x86 { background-color: rgba(168, 85, 247, 0.15); color: #c084fc; border: 1px solid rgba(168, 85, 247, 0.3); font-size: 10px; padding: 1px 4px; border-radius: 3px; font-weight: 600; }
        .badge-mismatch { background-color: rgba(239, 68, 68, 0.2); color: #f87171; border: 1px solid rgba(239, 68, 68, 0.5); font-size: 10px; padding: 1px 4px; border-radius: 3px; font-weight: 700; animation: pulse 2s infinite; }
        
        .tooltip { position: relative; display: inline-block; }
        .tooltip .tooltiptext { visibility: hidden; width: 170px; background-color: #18181b; color: #d4d4d8; text-align: center; border-radius: 6px; padding: 6px; position: absolute; z-index: 50; bottom: 125%; left: 50%; margin-left: -85px; opacity: 0; transition: opacity 0.2s; font-size: 11px; border: 1px solid #3f3f46; box-shadow: 0 4px 12px rgba(0,0,0,0.5); pointer-events: none; }
        .tooltip:hover .tooltiptext { visibility: visible; opacity: 1; }
    </style>
</head>
<body class="flex items-center justify-center min-h-screen">
    <div class="main-window w-full max-w-5xl h-[560px] flex flex-col text-zinc-300 rounded-lg shadow-2xl relative">
)HTML_PART1";

    html_content += R"HTML_PART2(
        <header class="flex items-center justify-between px-3 py-2 border-b border-zinc-800/80 flex-shrink-0 bg-zinc-950/90">
            <div class="flex items-center gap-2">
                <i class="ph-bold ph-planet text-cyan-400 text-lg"></i>
                <h1 class="font-bold tracking-widest text-white text-sm">MOON INJECTOR</h1>
            </div>
            <div class="flex items-center gap-2">
                <button id="open-crash-reports-btn" class="btn text-xs py-1 px-2.5 rounded-md hover:border-amber-400 text-zinc-300" title="crash reports and diagnostics"><i class="ph ph-shield-warning text-amber-400"></i>crashes</button>
                <button id="inspect-modules-btn" class="btn text-xs py-1 px-2.5 rounded-md text-zinc-300 hover:border-cyan-400 hidden" title="view and eject loaded modules"><i class="ph ph-browsers text-cyan-400"></i>modules</button>
                <button id="suspend-proc-btn" class="btn text-xs py-1 px-2 rounded-md hidden hover:border-cyan-400 text-zinc-300" title="suspend or resume process"><i class="ph ph-pause" id="suspend-btn-icon"></i><span id="suspend-btn-text">suspend</span></button>
                <button id="kill-proc-btn" class="btn text-xs py-1 px-2 rounded-md text-red-400 hover:border-red-500 hidden" title="terminate process"><i class="ph ph-x-circle"></i>kill</button>
                <button id="quit-btn" class="text-zinc-500 hover:text-red-500 transition p-1 ml-1" title="close injector"><i class="ph ph-power text-lg"></i></button>
            </div>
        </header>

        <div class="flex-grow grid grid-cols-12 gap-2 p-2 overflow-hidden">
            <div class="col-span-3 flex flex-col border border-zinc-800/80 bg-zinc-950/40 p-2 rounded-md">
                <h2 class="text-xs font-semibold text-zinc-400 mb-2 flex-shrink-0 flex items-center justify-between tracking-wider">
                    <span class="flex items-center gap-1.5"><i class="ph ph-stack text-cyan-400"></i>WORKSPACES</span>
                    <div class="flex items-center gap-1.5">
                        <button id="refresh-workspaces-btn" class="text-zinc-500 hover:text-cyan-400 transition" title="refresh"><i class="ph ph-arrows-clockwise text-base"></i></button>
                        <button id="add-workspace-btn" class="text-zinc-500 hover:text-cyan-400 transition" title="new workspace"><i class="ph ph-plus-circle text-base"></i></button>
                    </div>
                </h2>
                <div id="workspace-list" class="flex-grow overflow-y-auto border border-zinc-800/80 bg-black/40 p-1 font-mono text-xs space-y-1 rounded-sm">
                </div>
                <div class="grid grid-cols-2 gap-1.5 pt-2 flex-shrink-0">
                    <button id="rename-workspace-btn" class="btn text-xs py-1 rounded"><i class="ph ph-pencil-simple"></i>rename</button>
                    <button id="delete-workspace-btn" class="btn text-xs py-1 rounded text-red-400 hover:border-red-500"><i class="ph ph-trash"></i>delete</button>
                </div>
            </div>

            <div class="col-span-5 flex flex-col border border-zinc-800/80 bg-zinc-950/40 p-2 rounded-md min-h-0">
                <div class="flex items-center justify-between mb-2 flex-shrink-0">
                    <label class="text-xs font-semibold text-zinc-400 flex items-center gap-1.5 tracking-wider"><i class="ph ph-cpu text-cyan-400"></i>PROCESSES</label>
                    <div class="flex items-center gap-1.5">
                        <input type="search" id="proc-filter" placeholder="filter name or pid..." class="w-36 bg-zinc-900 border border-zinc-700/80 px-2 py-0.5 text-xs rounded focus:outline-none focus:border-cyan-500 transition text-zinc-200">
                        <button id="refresh-procs" class="text-zinc-500 hover:text-cyan-400 transition" title="refresh processes"><i class="ph ph-arrows-clockwise text-base"></i></button>
                    </div>
                </div>
                <div id="proc-list" class="flex-grow overflow-y-auto min-h-0 border border-zinc-800/80 bg-black/40 p-1 font-mono text-xs rounded-sm space-y-0.5">
                </div>
                <div class="flex-shrink-0 border border-zinc-800/80 bg-black/30 p-2 rounded-md mt-2">
                    <div class="flex items-center justify-between mb-1">
                        <h2 class="text-xs font-semibold text-zinc-400 flex items-center gap-1.5 tracking-wider"><i class="ph ph-terminal-window text-cyan-400"></i>STATUS LOG</h2>
                        <div class="flex items-center gap-1.5">
                            <button id="copy-log-btn" class="text-zinc-500 hover:text-cyan-400 transition" title="copy logs to clipboard"><i class="ph ph-copy text-sm"></i></button>
                            <button id="clear-log-btn" class="text-zinc-500 hover:text-red-400 transition" title="clear status log"><i class="ph ph-trash text-sm"></i></button>
                        </div>
                    </div>
                    <div id="status-log" class="h-20 overflow-y-auto bg-black/60 p-1.5 font-mono text-xs space-y-1 text-zinc-500 rounded border border-zinc-900">
                    </div>
                </div>
            </div>

            <div class="col-span-4 flex flex-col border border-zinc-800/80 bg-zinc-950/40 p-2 rounded-md">
                <div class="flex items-center justify-between mb-2 flex-shrink-0">
                    <h2 id="dll-list-header" class="text-xs font-semibold text-zinc-400 flex items-center gap-1.5 tracking-wider truncate"><i class="ph ph-file-code text-cyan-400"></i>INJECTION LIST</h2>
                    <button id="save-changes-btn" class="btn text-xs py-0.5 px-2 rounded"><i class="ph ph-floppy-disk"></i>save</button>
                </div>
                <div id="dll-list" class="flex-grow overflow-y-auto border border-zinc-800/80 bg-black/40 p-1 font-mono text-xs space-y-1 rounded-sm">
                </div>
                <div class="grid grid-cols-3 gap-1.5 pt-2 flex-shrink-0">
                    <button id="add-dll-btn" class="btn text-xs py-1 rounded"><i class="ph ph-plus"></i>add</button>
                    <button id="toggle-all-dlls-btn" class="btn text-xs py-1 rounded"><i class="ph ph-checks"></i>all</button>
                    <button id="clear-dlls-btn" class="btn text-xs py-1 rounded text-red-400 hover:border-red-500"><i class="ph ph-trash"></i>clear</button>
                </div>
            </div>
        </div>
)HTML_PART2";

    html_content += R"HTML_PART3(
        <footer class="flex items-stretch border-t border-zinc-800/80 flex-shrink-0 bg-zinc-950/90">
            <div class="w-3/5 p-2 flex flex-col justify-center border-r border-zinc-800/80">
                <div class="flex justify-between items-center mb-1">
                    <label class="text-xs text-zinc-500">injection method</label>
                    <div class="flex items-center gap-2">
                        <input type="checkbox" id="auto-inject-check" class="bg-zinc-900 border-zinc-700 rounded text-cyan-500 focus:ring-0">
                        <label for="auto-inject-check" class="text-xs text-zinc-400 select-none cursor-pointer">auto inject</label>
                    </div>
                </div>
                <div class="relative w-full" id="custom-select">
                    <button id="select-button" class="w-full bg-zinc-900 border border-zinc-700/80 p-1.5 text-xs text-left flex justify-between items-center rounded focus:outline-none focus:border-cyan-500 transition">
                        <span id="selected-value" class="text-zinc-200 font-medium">standard injection</span>
                        <i class="ph ph-caret-down text-zinc-500"></i>
                    </button>
                    <div id="options-panel" class="absolute bottom-full mb-1 w-full bg-zinc-900 border border-zinc-700 rounded shadow-xl z-20 hidden text-xs">
                        <div class="option p-1.5 hover:bg-zinc-800 cursor-pointer text-zinc-200" data-value="standard">standard injection (createremotethread)</div>
                        <div class="option p-1.5 hover:bg-zinc-800 cursor-pointer text-zinc-200" data-value="apc">apc queue injection stealth</div>
                        <div class="option p-1.5 hover:bg-zinc-800 cursor-pointer text-zinc-200" data-value="hijack">thread context hijack stealth</div>
                        <div class="option p-1.5 hover:bg-zinc-800 cursor-pointer text-cyan-400" data-value="blackbone">blackbone manual map</div>
                        <div class="option p-1.5 hover:bg-zinc-800 cursor-pointer text-cyan-400" data-value="prommap">pro manual map (stealth imports)</div>
                        <div class="option p-1.5 hover:bg-zinc-800 cursor-pointer text-zinc-200" data-value="pureil">pure il net clr</div>
                        <div class="option p-1.5 hover:bg-zinc-800 cursor-pointer text-amber-400 font-medium" data-value="kstandard">kernel standard [kernel driver]</div>
                        <div class="option p-1.5 hover:bg-zinc-800 cursor-pointer text-amber-400 font-medium" data-value="kmmap">kernel manual map [kernel driver]</div>
                    </div>
                </div>
                <div id="blackbone-options" class="mt-1.5 space-x-3 hidden">
                    <div class="tooltip inline-flex items-center gap-1.5">
                        <input type="checkbox" id="erase-pe-check" class="bg-zinc-900 border-zinc-700 rounded text-cyan-500 focus:ring-0">
                        <label for="erase-pe-check" class="text-xs text-zinc-400 select-none cursor-pointer">erase pe</label>
                        <span class="tooltiptext">wipes pe headers from memory</span>
                    </div>
                    <div class="tooltip inline-flex items-center gap-1.5">
                        <input type="checkbox" id="hide-module-check" class="bg-zinc-900 border-zinc-700 rounded text-cyan-500 focus:ring-0">
                        <label for="hide-module-check" class="text-xs text-zinc-400 select-none cursor-pointer">hide module</label>
                        <span class="tooltiptext">unlinks the module from peb</span>
                    </div>
                </div>
                <div id="il-options" class="mt-1.5 space-y-1 hidden flex-col w-full">
                    <input type="text" id="il-version" placeholder="net version eg v4030319" class="w-full bg-zinc-900 border border-zinc-700 px-2 py-0.5 text-xs rounded text-zinc-200">
                    <input type="text" id="il-method" placeholder="namespace class method" class="w-full bg-zinc-900 border border-zinc-700 px-2 py-0.5 text-xs rounded text-zinc-200">
                    <input type="text" id="il-args" placeholder="arguments" class="w-full bg-zinc-900 border border-zinc-700 px-2 py-0.5 text-xs rounded text-zinc-200">
                </div>
            </div>
            <div class="w-2/5 p-2 flex items-center justify-center">
                <button id="inject-btn" class="btn-primary w-full h-full text-sm font-bold tracking-widest rounded-md flex items-center justify-center gap-2"><i class="ph-bold ph-rocket-launch text-lg"></i>INJECT</button>
            </div>
        </footer>
    </div>
)HTML_PART3";

    html_content += R"HTML_PART4(
    <div id="workspace-modal-overlay" class="fixed inset-0 bg-black/60 z-40 hidden items-center justify-center backdrop-blur-sm">
        <div id="workspace-modal" class="bg-zinc-900 border border-zinc-700 p-4 rounded-lg shadow-2xl w-96">
            <h3 id="modal-title" class="text-sm font-bold text-white mb-3">create workspace</h3>
            <input type="text" id="workspace-name-input" class="w-full bg-zinc-800 border border-zinc-700 p-2 text-xs rounded focus:outline-none focus:border-cyan-500 transition mb-3 text-zinc-200" placeholder="workspace name...">
            <div class="flex justify-end gap-2">
                <button id="modal-cancel-btn" class="btn text-xs py-1 px-3 rounded">cancel</button>
                <button id="modal-save-btn" class="btn-primary text-xs py-1 px-3 rounded">save</button>
            </div>
        </div>
    </div>

    <div id="modules-modal-overlay" class="fixed inset-0 bg-black/70 z-40 hidden items-center justify-center backdrop-blur-sm p-4">
        <div class="bg-zinc-900 border border-zinc-700 p-4 rounded-lg shadow-2xl w-full max-w-2xl h-[420px] flex flex-col">
            <div class="flex items-center justify-between pb-2 border-b border-zinc-800 flex-shrink-0">
                <div class="flex items-center gap-2">
                    <i class="ph ph-browsers text-cyan-400 text-lg"></i>
                    <h3 id="modules-modal-title" class="text-sm font-bold text-white">modules explorer</h3>
                </div>
                <div class="flex items-center gap-2">
                    <input type="search" id="modules-filter" placeholder="filter modules..." class="w-40 bg-zinc-800 border border-zinc-700 px-2 py-0.5 text-xs rounded text-zinc-200 focus:outline-none focus:border-cyan-500">
                    <button id="refresh-modules-btn" class="btn text-xs py-0.5 px-2 rounded" title="refresh"><i class="ph ph-arrows-clockwise"></i></button>
                    <button id="close-modules-btn" class="text-zinc-500 hover:text-white transition p-1"><i class="ph ph-x text-lg"></i></button>
                </div>
            </div>
            <div id="modules-table-container" class="flex-grow overflow-y-auto mt-2 border border-zinc-800 bg-black/40 rounded p-1 font-mono text-xs">
                <table class="w-full text-left border-collapse">
                    <thead>
                        <tr class="text-zinc-500 border-b border-zinc-800 text-[11px]">
                            <th class="p-1">module name</th>
                            <th class="p-1">base address</th>
                            <th class="p-1">size</th>
                            <th class="p-1 text-right">action</th>
                        </tr>
                    </thead>
                    <tbody id="modules-table-body">
                    </tbody>
                </table>
            </div>
        </div>
    </div>

    <div id="crash-modal-overlay" class="fixed inset-0 bg-black/70 z-40 hidden items-center justify-center backdrop-blur-sm p-4">
        <div class="bg-zinc-900 border border-zinc-700 p-4 rounded-lg shadow-2xl w-full max-w-3xl h-[440px] flex flex-col">
            <div class="flex items-center justify-between pb-2 border-b border-zinc-800 flex-shrink-0">
                <div class="flex items-center gap-2">
                    <i class="ph ph-shield-warning text-amber-400 text-lg"></i>
                    <h3 class="text-sm font-bold text-white">crash reports & diagnostics</h3>
                </div>
                <div class="flex items-center gap-2">
                    <button id="open-crash-folder-btn" class="btn text-xs py-1 px-2.5 rounded"><i class="ph ph-folder-open"></i>open folder</button>
                    <button id="close-crash-btn" class="text-zinc-500 hover:text-white transition p-1"><i class="ph ph-x text-lg"></i></button>
                </div>
            </div>
            <div class="grid grid-cols-12 gap-2 flex-grow min-h-0 mt-2">
                <div class="col-span-4 border border-zinc-800 bg-black/40 rounded p-1 flex flex-col min-h-0">
                    <span class="text-[11px] font-semibold text-zinc-500 px-1 pb-1 flex-shrink-0">saved reports</span>
                    <div id="crash-reports-list" class="flex-grow overflow-y-auto font-mono text-xs space-y-1">
                    </div>
                </div>
                <div class="col-span-8 border border-zinc-800 bg-black/60 rounded p-2 flex flex-col min-h-0">
                    <div class="flex items-center justify-between pb-1 flex-shrink-0">
                        <span id="crash-viewer-title" class="text-xs font-mono text-zinc-400">select a report to view details</span>
                        <button id="copy-crash-btn" class="btn text-xs py-0.5 px-2 rounded hidden"><i class="ph ph-copy"></i>copy</button>
                    </div>
                    <pre id="crash-viewer-content" class="flex-grow overflow-y-auto font-mono text-[11px] text-zinc-300 whitespace-pre-wrap select-text p-1 border border-zinc-900 bg-zinc-950/80 rounded"></pre>
                </div>
            </div>
        </div>
    </div>

    <div id="dll-info-modal-overlay" class="fixed inset-0 bg-black/70 z-40 hidden items-center justify-center backdrop-blur-sm p-4">
        <div class="bg-zinc-900 border border-zinc-700 p-4 rounded-lg shadow-2xl w-96 flex flex-col">
            <div class="flex items-center justify-between pb-2 border-b border-zinc-800">
                <h3 class="text-sm font-bold text-white flex items-center gap-1.5"><i class="ph ph-info text-cyan-400"></i>pe inspection</h3>
                <button id="close-dll-info-btn" class="text-zinc-500 hover:text-white transition p-1"><i class="ph ph-x text-lg"></i></button>
            </div>
            <div id="dll-info-content" class="py-3 font-mono text-xs space-y-2 text-zinc-300">
            </div>
            <div class="flex justify-end pt-2 border-t border-zinc-800">
                <button id="close-dll-info-btn2" class="btn text-xs py-1 px-3 rounded">close</button>
            </div>
        </div>
    </div>
)HTML_PART4";

    html_content += R"HTML_PART5(
    <script>
        let selectedPid = null;
        let selectedProcName = '';
        let selectedProcArch = '';
        let isProcSuspended = false;
        let dlls = [];
        let workspaces = [];
        let selectedWorkspaceId = null;
        let injectMethod = 'standard';
        let currentLoadedModules = [];

        const procListDiv = document.getElementById('proc-list');
        const dllListDiv = document.getElementById('dll-list');
        const statusLogDiv = document.getElementById('status-log');
        const blackboneOptionsDiv = document.getElementById('blackbone-options');
        const erasePECheckbox = document.getElementById('erase-pe-check');
        const hideModuleCheckbox = document.getElementById('hide-module-check');
        const autoInjectCheckbox = document.getElementById('auto-inject-check');
        const selectButton = document.getElementById('select-button');
        const optionsPanel = document.getElementById('options-panel');
        const selectedValueSpan = document.getElementById('selected-value');
        const workspaceListDiv = document.getElementById('workspace-list');
        const dllListHeader = document.getElementById('dll-list-header');

        const inspectModulesBtn = document.getElementById('inspect-modules-btn');
        const suspendProcBtn = document.getElementById('suspend-proc-btn');
        const suspendBtnIcon = document.getElementById('suspend-btn-icon');
        const suspendBtnText = document.getElementById('suspend-btn-text');
        const killProcBtn = document.getElementById('kill-proc-btn');

        const workspaceModalOverlay = document.getElementById('workspace-modal-overlay');
        const modalTitle = document.getElementById('modal-title');
        const workspaceNameInput = document.getElementById('workspace-name-input');
        const modalCancelBtn = document.getElementById('modal-cancel-btn');
        const modalSaveBtn = document.getElementById('modal-save-btn');
        let modalMode = 'add';
        let renameId = null;

        const modulesModalOverlay = document.getElementById('modules-modal-overlay');
        const modulesModalTitle = document.getElementById('modules-modal-title');
        const modulesFilterInput = document.getElementById('modules-filter');
        const modulesTableBody = document.getElementById('modules-table-body');
        const refreshModulesBtn = document.getElementById('refresh-modules-btn');
        const closeModulesBtn = document.getElementById('close-modules-btn');

        const crashModalOverlay = document.getElementById('crash-modal-overlay');
        const openCrashReportsBtn = document.getElementById('open-crash-reports-btn');
        const openCrashFolderBtn = document.getElementById('open-crash-folder-btn');
        const closeCrashBtn = document.getElementById('close-crash-btn');
        const crashReportsListDiv = document.getElementById('crash-reports-list');
        const crashViewerTitle = document.getElementById('crash-viewer-title');
        const crashViewerContent = document.getElementById('crash-viewer-content');
        const copyCrashBtn = document.getElementById('copy-crash-btn');

        const dllInfoModalOverlay = document.getElementById('dll-info-modal-overlay');
        const dllInfoContent = document.getElementById('dll-info-content');
        const closeDllInfoBtn = document.getElementById('close-dll-info-btn');
        const closeDllInfoBtn2 = document.getElementById('close-dll-info-btn2');

        function formatCurrentTime() {
            const date = new Date();
            const hours = String(date.getHours()).padStart(2, '0');
            const minutes = String(date.getMinutes()).padStart(2, '0');
            const seconds = String(date.getSeconds()).padStart(2, '0');
            return `${hours}:${minutes}:${seconds}`;
        }

        function logStatus(message, type = 'info') {
            const p = document.createElement('p');
            let colorClass = 'text-zinc-500';
            if (type === 'success') colorClass = 'text-emerald-400';
            if (type === 'error') colorClass = 'text-rose-400';
            if (type === 'selection') colorClass = 'text-cyan-400';
            if (type === 'warning') colorClass = 'text-amber-400';
            p.innerHTML = `<span class="text-zinc-600">[${formatCurrentTime()}]</span> <span class="${colorClass}">${message}</span>`;
            statusLogDiv.appendChild(p);
            statusLogDiv.scrollTop = statusLogDiv.scrollHeight;
        }

        async function initializeApp() {
            logStatus('moon injector v5.0 pro initialized ready for injection');
            refreshProcesses();
            await loadWorkspaces();
        }

        async function loadWorkspaces() {
            const workspacesJson = await window.native_getWorkspaces();
            try {
                workspaces = JSON.parse(workspacesJson);
            } catch (e) {
                logStatus('error loading workspaces from database', 'error');
                workspaces = [];
            }
            renderWorkspaces();
            if (workspaces.length > 0) {
                const stillExists = workspaces.find(w => w.id === selectedWorkspaceId);
                if (stillExists) {
                    await selectWorkspace(stillExists.id, stillExists.name);
                } else {
                    await selectWorkspace(workspaces[0].id, workspaces[0].name);
                }
            } else {
                selectedWorkspaceId = null;
                dlls = [];
                renderDlls();
                dllListHeader.innerHTML = `<i class="ph ph-file-code text-cyan-400"></i>INJECTION LIST`;
            }
        }
)HTML_PART5";

    html_content += R"HTML_PART6(
        function renderWorkspaces() {
            workspaceListDiv.innerHTML = '';
            workspaces.forEach(ws => {
                const item = document.createElement('div');
                item.className = 'list-item';
                item.dataset.id = ws.id;
                item.textContent = ws.name;
                if (ws.id === selectedWorkspaceId) {
                    item.classList.add('selected');
                }
                item.onclick = () => selectWorkspace(ws.id, ws.name);
                workspaceListDiv.appendChild(item);
            });
        }

        async function selectWorkspace(id, name) {
            selectedWorkspaceId = id;
            logStatus(`switched workspace to ${name}`, 'selection');
            dllListHeader.innerHTML = `<i class="ph ph-file-code text-cyan-400"></i>${name}`;
            renderWorkspaces();
            await loadDllsForWorkspace(id);
        }

        function openWorkspaceModal(mode, id = null, currentName = '') {
            modalMode = mode;
            renameId = id;
            modalTitle.textContent = mode === 'add' ? 'create workspace' : 'rename workspace';
            workspaceNameInput.value = currentName;
            workspaceModalOverlay.classList.remove('hidden');
            workspaceModalOverlay.classList.add('flex');
            workspaceNameInput.focus();
        }

        function closeWorkspaceModal() {
            workspaceModalOverlay.classList.add('hidden');
            workspaceModalOverlay.classList.remove('flex');
        }

        async function saveWorkspace() {
            const name = workspaceNameInput.value.trim();
            if (!name) return;
            if (modalMode === 'add') {
                const newId = await window.native_addWorkspace(name);
                if (newId > 0) {
                    logStatus(`created workspace ${name}`, 'success');
                    await loadWorkspaces();
                    await selectWorkspace(newId, name);
                } else {
                    logStatus(`workspace ${name} already exists`, 'error');
                }
            } else if (modalMode === 'rename') {
                await window.native_renameWorkspace(renameId, name);
                logStatus(`renamed workspace to ${name}`, 'success');
                await loadWorkspaces();
            }
            closeWorkspaceModal();
        }

        async function deleteWorkspace() {
            if (!selectedWorkspaceId) {
                logStatus('no workspace selected to delete', 'error');
                return;
            }
            if (workspaces.length <= 1) {
                logStatus('cannot delete the last workspace', 'error');
                return;
            }
            const currentWs = workspaces.find(w => w.id === selectedWorkspaceId);
            const confirmed = await window.native_messageBoxYesNo("confirm deletion", `are you sure you want to delete workspace ${currentWs.name} and all its dlls`);
            if (confirmed) {
                await window.native_deleteWorkspace(selectedWorkspaceId);
                logStatus(`deleted workspace ${currentWs.name}`, 'success');
                selectedWorkspaceId = null;
                await loadWorkspaces();
            }
        }

        async function refreshProcesses() {
            const procsJson = await window.native_getProcesses();
            let processes = [];
            try {
                processes = JSON.parse(procsJson);
            } catch (e) {
                logStatus('error parsing process list', 'error');
            }
            procListDiv.innerHTML = '';
            processes.sort((a,b) => a.name.localeCompare(b.name)).forEach(proc => {
                const item = document.createElement('div');
                item.className = 'list-item';
                item.dataset.pid = proc.pid;
                item.dataset.arch = proc.arch;
                item.dataset.name = proc.name;
                const badgeClass = proc.arch === 'x64' ? 'badge-arch-x64' : (proc.arch === 'x86' ? 'badge-arch-x86' : 'text-zinc-600');
                item.innerHTML = `
                    <span class="truncate pr-2 font-medium">${proc.name}</span>
                    <span class="flex items-center gap-1.5 flex-shrink-0">
                        <span class="${badgeClass}">${proc.arch}</span>
                        <span class="text-zinc-500">pid ${proc.pid}</span>
                    </span>`;
                item.onclick = () => selectProcess(item, proc.pid, proc.name, proc.arch);
                if (proc.pid === selectedPid) {
                    item.classList.add('selected');
                }
                procListDiv.appendChild(item);
            });
            logStatus(`enumerated ${processes.length} active processes`, 'info');
        }

        function selectProcess(element, pid, name, arch) {
            const currentSelected = procListDiv.querySelector('.selected');
            if(currentSelected) currentSelected.classList.remove('selected');
            element.classList.add('selected');
            selectedPid = pid;
            selectedProcName = name;
            selectedProcArch = arch;
            isProcSuspended = false;
            suspendBtnText.textContent = 'suspend';
            suspendBtnIcon.className = 'ph ph-pause';

            inspectModulesBtn.classList.remove('hidden');
            suspendProcBtn.classList.remove('hidden');
            killProcBtn.classList.remove('hidden');

            logStatus(`attached target ${name} (pid ${pid} | ${arch})`, 'selection');
            renderDlls();

            if (autoInjectCheckbox.checked) {
                logStatus('auto inject triggered', 'selection');
                doInject();
            }
        }

        function filterProcesses() {
            const filterText = document.getElementById('proc-filter').value.toLowerCase();
            const items = procListDiv.getElementsByClassName('list-item');
            for(let i=0; i < items.length; i++) {
                const name = items[i].dataset.name ? items[i].dataset.name.toLowerCase() : '';
                const pid = items[i].dataset.pid ? items[i].dataset.pid : '';
                if (name.includes(filterText) || pid.includes(filterText)) {
                    items[i].style.display = 'flex';
                } else {
                    items[i].style.display = 'none';
                }
            }
        }
)HTML_PART6";

    html_content += R"HTML_PART7(
        async function inspectDll(path) {
            try {
                const infoJson = await window.native_inspectDll(path);
                return JSON.parse(infoJson);
            } catch(e) {
                return { architecture: 'unknown', isValidExecutableImage: false, isDebugBuild: false, hasPdbFile: false, pdbFilePath: '', fileSizeBytes: 0 };
            }
        }

        async function addDll() {
            const path = await window.native_openDllDialog();
            if (path) {
                await addDllToList(path);
                logStatus('dll added click save to persist workspace', 'info');
            }
        }

        async function addDllToList(path) {
            const dllName = path.substring(path.lastIndexOf('\\') + 1);
            if (dlls.some(d => d.path === path)) { return; }
            const peInfo = await inspectDll(path);
            dlls.push({
                path: path,
                name: dllName,
                selected: true,
                architecture: peInfo.architecture || 'unknown',
                isDebugBuild: peInfo.isDebugBuild || false,
                hasPdbFile: peInfo.hasPdbFile || false,
                pdbFilePath: peInfo.pdbFilePath || '',
                fileSizeBytes: peInfo.fileSizeBytes || 0
            });
            renderDlls();
        }

        async function loadDllsForWorkspace(workspaceId) {
            dlls = [];
            const dllsJson = await window.native_getDlls(workspaceId);
            let savedDlls = [];
            try {
                 savedDlls = JSON.parse(dllsJson);
            } catch(e) {
                logStatus('error loading dll list', 'error');
            }
            for (const path of savedDlls) {
                await addDllToList(path);
            }
            logStatus(`loaded ${dlls.length} dlls in workspace`, 'info');
            renderDlls();
        }

        function renderDlls() {
            dllListDiv.innerHTML = '';
            dlls.forEach((dll, index) => {
                const item = document.createElement('div');
                item.className = 'list-item group';
                if(dll.selected) item.classList.add('selected');
                const checkIcon = dll.selected ? 'ph-fill ph-check-square text-cyan-400' : 'ph ph-square';
                let archBadge = '';
                if (dll.architecture === 'x64') archBadge = '<span class="badge-arch-x64">x64</span>';
                else if (dll.architecture === 'x86') archBadge = '<span class="badge-arch-x86">x86</span>';

                let mismatchWarning = '';
                if (selectedProcArch && dll.architecture !== 'unknown' && selectedProcArch !== 'n/a' && dll.architecture !== selectedProcArch) {
                    mismatchWarning = `<span class="badge-mismatch" title="architecture mismatch dll is ${dll.architecture} but target is ${selectedProcArch}">MISMATCH</span>`;
                }

                item.innerHTML = `
                    <div class="flex items-center gap-1.5 truncate pr-2 cursor-pointer" onclick="toggleDll(${index})">
                        <i class="${checkIcon}"></i>
                        <span class="truncate">${dll.name}</span>
                    </div>
                    <div class="flex items-center gap-1.5 flex-shrink-0">
                        ${mismatchWarning}
                        ${archBadge}
                        <i class="ph ph-info text-zinc-600 hover:text-cyan-400 cursor-pointer p-0.5" onclick="showDllDetails(${index})" title="view pe header info"></i>
                        <i class="ph ph-x text-zinc-600 hover:text-rose-400 cursor-pointer p-0.5" onclick="removeDll(${index})" title="remove"></i>
                    </div>`;
                dllListDiv.appendChild(item);
            });
            if (dlls.length === 0) {
                 dllListDiv.innerHTML = '<div class="text-center text-zinc-600 text-xs py-6">no dlls in this workspace</div>';
            }
        }

        function toggleDll(index) {
            dlls[index].selected = !dlls[index].selected;
            renderDlls();
        }

        function toggleAllDlls() {
            if (dlls.length === 0) return;
            const anyUnselected = dlls.some(d => !d.selected);
            dlls.forEach(d => d.selected = anyUnselected);
            renderDlls();
        }

        function removeDll(index) {
            const removed = dlls.splice(index, 1);
            if (removed.length > 0) {
                logStatus(`removed ${removed[0].name} from list`, 'info');
                renderDlls();
            }
        }

        function clearDlls() {
            if (dlls.length === 0) return;
            dlls = [];
            logStatus('cleared all dlls click save to persist', 'info');
            renderDlls();
        }

        async function saveChanges() {
            if (!selectedWorkspaceId) {
                logStatus('cannot save no workspace selected', 'error');
                return;
            }
            const paths = dlls.map(d => d.path);
            const pathsStr = paths.join('_|_');
            await window.native_saveDlls(selectedWorkspaceId, pathsStr);
            logStatus('workspace synchronized and saved', 'success');
        }

        function showDllDetails(index) {
            const dll = dlls[index];
            if (!dll) return;
            const sizeKb = (dll.fileSizeBytes / 1024).toFixed(1);
            let compatibilityText = 'unknown';
            let compatibilityColor = 'text-zinc-400';
            if (selectedProcArch && dll.architecture !== 'unknown') {
                if (selectedProcArch === dll.architecture) {
                    compatibilityText = `compatible with ${selectedProcName} (${selectedProcArch})`;
                    compatibilityColor = 'text-emerald-400';
                } else {
                    compatibilityText = `MISMATCH target is ${selectedProcArch} but dll is ${dll.architecture}`;
                    compatibilityColor = 'text-rose-400 font-bold';
                }
            }
            dllInfoContent.innerHTML = `
                <div><span class="text-zinc-500">file name:</span> <span class="text-white">${dll.name}</span></div>
                <div><span class="text-zinc-500">architecture:</span> <span class="text-cyan-400 font-bold">${dll.architecture}</span></div>
                <div><span class="text-zinc-500">compatibility:</span> <span class="${compatibilityColor}">${compatibilityText}</span></div>
                <div><span class="text-zinc-500">build type:</span> <span class="text-zinc-200">${dll.isDebugBuild ? 'Debug Build' : 'Release Build'}</span></div>
                <div><span class="text-zinc-500">pdb symbols:</span> <span class="text-zinc-200">${dll.hasPdbFile ? 'Present' : 'Not Found'}</span></div>
                <div><span class="text-zinc-500">file size:</span> <span class="text-zinc-200">${sizeKb} KB</span></div>
                <div class="break-all text-[11px]"><span class="text-zinc-500">full path:</span> <span class="text-zinc-400">${dll.path}</span></div>
            `;
            dllInfoModalOverlay.classList.remove('hidden');
            dllInfoModalOverlay.classList.add('flex');
        }

        async function doInject() {
            if(!selectedPid) {
                logStatus('no process selected for injection', 'error');
                return;
            }
            const dllsToInject = dlls.filter(d => d.selected);
            if(dllsToInject.length === 0) {
                logStatus('no dlls selected for injection', 'error');
                return;
            }

            for (const dll of dllsToInject) {
                if (selectedProcArch && dll.architecture !== 'unknown' && selectedProcArch !== 'n/a' && dll.architecture !== selectedProcArch) {
                    logStatus(`safety abort ${dll.name} is ${dll.architecture} while target is ${selectedProcArch} cross arch crashes target`, 'error');
                    const bypass = await window.native_messageBoxYesNo("architecture mismatch warning", `dll ${dll.name} (${dll.architecture}) does not match target ${selectedProcName} (${selectedProcArch})\n\ninjecting cross-architecture dlls will crash the target process\n\ndo you still want to force inject`);
                    if (!bypass) {
                        logStatus('injection aborted by safety guard', 'warning');
                        return;
                    }
                }
            }

            const method = injectMethod;
            const erasePE = erasePECheckbox.checked;
            const hideModule = hideModuleCheckbox.checked;
            const netVer = document.getElementById('il-version').value;
            const ilMeth = document.getElementById('il-method').value;
            const ilArgs = document.getElementById('il-args').value;

            logStatus(`dispatching injection into ${selectedProcName} (pid ${selectedPid}) via ${selectedValueSpan.textContent}`);
            for (const dll of dllsToInject) {
                logStatus(`injecting ${dll.name}...`);
                const result = await window.native_inject(selectedPid, dll.path, method, erasePE, hideModule, netVer, ilMeth, ilArgs);
                if (result.toLowerCase().includes('success')) {
                    logStatus(`injected ${dll.name} successfully`, 'success');
                } else {
                    logStatus(`failed ${dll.name} error ${result}`, 'error');
                }
            }
        }
)HTML_PART7";

    html_content += R"HTML_PART8(
        async function openModulesExplorer() {
            if (!selectedPid) {
                logStatus('select a target process first', 'error');
                return;
            }
            modulesModalTitle.textContent = `modules explorer - ${selectedProcName} (pid ${selectedPid})`;
            modulesModalOverlay.classList.remove('hidden');
            modulesModalOverlay.classList.add('flex');
            await loadModules();
        }

        async function loadModules() {
            modulesTableBody.innerHTML = '<tr><td colspan="4" class="p-4 text-center text-zinc-500">reading target process modules...</td></tr>';
            const modulesJson = await window.native_getProcessModules(selectedPid);
            try {
                currentLoadedModules = JSON.parse(modulesJson);
            } catch (e) {
                currentLoadedModules = [];
            }
            renderModulesTable();
        }

        function renderModulesTable() {
            const filter = modulesFilterInput.value.toLowerCase();
            modulesTableBody.innerHTML = '';
            const filtered = currentLoadedModules.filter(m => m.moduleName.toLowerCase().includes(filter) || m.fullFilePath.toLowerCase().includes(filter));
            if (filtered.length === 0) {
                modulesTableBody.innerHTML = '<tr><td colspan="4" class="p-4 text-center text-zinc-600">no modules found</td></tr>';
                return;
            }
            filtered.forEach(mod => {
                const tr = document.createElement('tr');
                tr.className = 'border-b border-zinc-800/60 hover:bg-zinc-800/40 transition text-zinc-300';
                const sizeKb = (mod.moduleMemorySize / 1024).toFixed(0);
                tr.innerHTML = `
                    <td class="p-1 truncate max-w-[200px]" title="${mod.fullFilePath}">${mod.moduleName}</td>
                    <td class="p-1 text-cyan-400 font-mono text-[11px]">${mod.baseAddress}</td>
                    <td class="p-1 text-zinc-400 font-mono text-[11px]">${sizeKb} KB</td>
                    <td class="p-1 text-right">
                        <button class="btn text-[10px] py-0.5 px-2 rounded hover:border-rose-500 text-rose-400" onclick="ejectModule('${mod.moduleName}')">eject</button>
                    </td>`;
                modulesTableBody.appendChild(tr);
            });
        }

        async function ejectModule(moduleName) {
            if (!selectedPid) return;
            const confirm = await window.native_messageBoxYesNo("confirm unload", `are you sure you want to unload module ${moduleName} from ${selectedProcName}`);
            if (!confirm) return;
            logStatus(`attempting to unload module ${moduleName}...`, 'info');
            const success = await window.native_ejectModule(selectedPid, moduleName);
            if (success) {
                logStatus(`module ${moduleName} unloaded successfully`, 'success');
                await loadModules();
            } else {
                logStatus(`failed to unload module ${moduleName}`, 'error');
            }
        }

        async function toggleProcessSuspend() {
            if (!selectedPid) return;
            if (!isProcSuspended) {
                const success = await window.native_suspendProcess(selectedPid);
                if (success) {
                    isProcSuspended = true;
                    suspendBtnText.textContent = 'resume';
                    suspendBtnIcon.className = 'ph ph-play';
                    logStatus(`suspended process threads for ${selectedProcName}`, 'warning');
                } else {
                    logStatus(`failed to suspend process ${selectedProcName}`, 'error');
                }
            } else {
                const success = await window.native_resumeProcess(selectedPid);
                if (success) {
                    isProcSuspended = false;
                    suspendBtnText.textContent = 'suspend';
                    suspendBtnIcon.className = 'ph ph-pause';
                    logStatus(`resumed process threads for ${selectedProcName}`, 'success');
                } else {
                    logStatus(`failed to resume process ${selectedProcName}`, 'error');
                }
            }
        }

        async function terminateProcess() {
            if (!selectedPid) return;
            const confirm = await window.native_messageBoxYesNo("kill process", `are you sure you want to terminate ${selectedProcName} (pid ${selectedPid})`);
            if (!confirm) return;
            const success = await window.native_terminateProcess(selectedPid);
            if (success) {
                logStatus(`terminated process ${selectedProcName}`, 'success');
                selectedPid = null;
                selectedProcName = '';
                inspectModulesBtn.classList.add('hidden');
                suspendProcBtn.classList.add('hidden');
                killProcBtn.classList.add('hidden');
                refreshProcesses();
            } else {
                logStatus(`failed to terminate process ${selectedProcName}`, 'error');
            }
        }

        async function openCrashReportsModal() {
            crashModalOverlay.classList.remove('hidden');
            crashModalOverlay.classList.add('flex');
            await loadCrashReports();
        }

        async function loadCrashReports() {
            crashReportsListDiv.innerHTML = '<div class="text-zinc-600 text-xs p-2">scanning reports...</div>';
            const reportsJson = await window.native_getCrashReports();
            let reports = [];
            try {
                reports = JSON.parse(reportsJson);
            } catch (e) {
                reports = [];
            }
            crashReportsListDiv.innerHTML = '';
            if (reports.length === 0) {
                crashReportsListDiv.innerHTML = '<div class="text-zinc-600 text-xs p-2">no crash reports recorded</div>';
                crashViewerTitle.textContent = 'no reports available';
                crashViewerContent.textContent = 'run with crash monitor to record crash dumps and callstack reports automatically';
                copyCrashBtn.classList.add('hidden');
                return;
            }
            reports.forEach(reportName => {
                const item = document.createElement('div');
                item.className = 'list-item text-[11px] truncate';
                item.textContent = reportName;
                item.onclick = () => selectCrashReport(reportName);
                crashReportsListDiv.appendChild(item);
            });
            if (reports.length > 0) {
                selectCrashReport(reports[0]);
            }
        }

        async function selectCrashReport(reportName) {
            crashViewerTitle.textContent = reportName;
            copyCrashBtn.classList.remove('hidden');
            const content = await window.native_readCrashReport(reportName);
            crashViewerContent.textContent = content;
        }

        selectButton.onclick = () => optionsPanel.classList.toggle('hidden');
        document.querySelectorAll('.option').forEach(option => {
            option.onclick = () => {
                injectMethod = option.getAttribute('data-value');
                selectedValueSpan.textContent = option.textContent;
                optionsPanel.classList.add('hidden');

                blackboneOptionsDiv.classList.toggle('hidden', injectMethod !== 'blackbone' && injectMethod !== 'prommap');
                blackboneOptionsDiv.classList.toggle('flex', injectMethod === 'blackbone' || injectMethod === 'prommap');

                const ilOptionsDiv = document.getElementById('il-options');
                ilOptionsDiv.classList.toggle('hidden', injectMethod !== 'pureil');
                ilOptionsDiv.classList.toggle('flex', injectMethod === 'pureil');

                if (['kstandard', 'kmmap'].includes(injectMethod)) {
                    logStatus('kernel methods require driver test signing and administrator elevation', 'warning');
                }
            };
        });

        document.addEventListener('click', (e) => {
            if (!document.getElementById('custom-select').contains(e.target)) {
                optionsPanel.classList.add('hidden');
            }
        });

        document.getElementById('refresh-procs').onclick = refreshProcesses;
        document.getElementById('proc-filter').addEventListener('input', filterProcesses);
        document.getElementById('refresh-workspaces-btn').onclick = loadWorkspaces;
        document.getElementById('add-workspace-btn').onclick = () => openWorkspaceModal('add');
        document.getElementById('rename-workspace-btn').onclick = () => {
             if (!selectedWorkspaceId) {
                logStatus('no workspace selected to rename', 'error');
                return;
            }
            const currentWs = workspaces.find(w => w.id === selectedWorkspaceId);
            openWorkspaceModal('rename', selectedWorkspaceId, currentWs.name);
        };
        document.getElementById('delete-workspace-btn').onclick = deleteWorkspace;
        document.getElementById('add-dll-btn').onclick = addDll;
        document.getElementById('toggle-all-dlls-btn').onclick = toggleAllDlls;
        document.getElementById('clear-dlls-btn').onclick = clearDlls;
        document.getElementById('save-changes-btn').onclick = saveChanges;
        document.getElementById('inject-btn').onclick = doInject;
        document.getElementById('quit-btn').onclick = () => window.native_quit();

        inspectModulesBtn.onclick = openModulesExplorer;
        refreshModulesBtn.onclick = loadModules;
        modulesFilterInput.addEventListener('input', renderModulesTable);
        closeModulesBtn.onclick = () => {
            modulesModalOverlay.classList.add('hidden');
            modulesModalOverlay.classList.remove('flex');
        };

        suspendProcBtn.onclick = toggleProcessSuspend;
        killProcBtn.onclick = terminateProcess;

        openCrashReportsBtn.onclick = openCrashReportsModal;
        openCrashFolderBtn.onclick = () => window.native_openCrashFolder();
        closeCrashBtn.onclick = () => {
            crashModalOverlay.classList.add('hidden');
            crashModalOverlay.classList.remove('flex');
        };
        copyCrashBtn.onclick = async () => {
            const content = crashViewerContent.textContent;
            await window.native_copyToClipboard(content);
            logStatus('crash report copied to clipboard', 'success');
        };

        closeDllInfoBtn.onclick = () => {
            dllInfoModalOverlay.classList.add('hidden');
            dllInfoModalOverlay.classList.remove('flex');
        };
        closeDllInfoBtn2.onclick = () => {
            dllInfoModalOverlay.classList.add('hidden');
            dllInfoModalOverlay.classList.remove('flex');
        };

        modalCancelBtn.onclick = closeWorkspaceModal;
        modalSaveBtn.onclick = saveWorkspace;
        workspaceNameInput.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') saveWorkspace();
            if (e.key === 'Escape') closeWorkspaceModal();
        });

        document.getElementById('copy-log-btn').onclick = async () => {
            const allLogs = Array.from(statusLogDiv.querySelectorAll('p')).map(p => p.textContent).join('\n');
            await window.native_copyToClipboard(allLogs);
            logStatus('all status logs copied to clipboard', 'success');
        };

        document.getElementById('clear-log-btn').onclick = () => {
            statusLogDiv.innerHTML = '';
            logStatus('status log cleared', 'info');
        };

        initializeApp();
    </script>
</body>
</html>
)HTML_PART8";

    g_ultralight_controller->LoadHTML(html_content);

    bool done = false;
    bool show_app = true;
    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ShowApp(&show_app);

        const float clear_color_with_alpha[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        ID3D11RenderTargetView *mainRenderTargetView = GetMainRenderTargetView();
        ID3D11DeviceContext *context = GetImmediateContext();

        if (mainRenderTargetView && context)
        {
            context->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
            context->ClearRenderTargetView(mainRenderTargetView, clear_color_with_alpha);
        }

        ImGui::Render();
        if (ImGui::GetDrawData())
        {
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        IDXGISwapChain *swapChain = GetSwapChain();
        if (swapChain)
        {
            swapChain->Present(1, 0);
        }
    }

    g_ultralight_controller.reset();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    CleanupWindow(hInstance, className);

    return 0;
}
