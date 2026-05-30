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
#include <filesystem>
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

std::string escapeJsonString(const std::string &input)
{
    std::stringstream ss;
    for (char c : input)
    {
        switch (c)
        {
        case '\\':
            ss << "\\\\";
            break;
        case '"':
            ss << "\\\"";
            break;
        case '/':
            ss << "\\/";
            break;
        case '\b':
            ss << "\\b";
            break;
        case '\f':
            ss << "\\f";
            break;
        case '\n':
            ss << "\\n";
            break;
        case '\r':
            ss << "\\r";
            break;
        case '\t':
            ss << "\\t";
            break;
        default:
            if (c >= 0 && c < 32)
            {
                ss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                   << static_cast<int>(static_cast<unsigned char>(c));
            }
            else
            {
                ss << c;
            }
            break;
        }
    }
    return ss.str();
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
        MessageBoxA(NULL, e.what(), "Database Error", MB_OK | MB_ICONERROR);
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
        std::string error_msg = "An exception occurred during Ultralight initialization:\n\n";
        error_msg += e.what();
        error_msg += "\n\nThis usually means the 'resources' folder is missing from the executable's directory.";
        MessageBoxA(NULL, error_msg.c_str(), "Ultralight Initialization Failed", MB_OK | MB_ICONERROR);
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
            const char *filePath = tinyfd_openFileDialog("Select DLL", "", 1, filterPatterns, "DLL Files", 0);
            if (filePath)
            {
                return ultralight::JSValue(filePath);
            }
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
                    {
                        paths.push_back(token);
                    }
                    s.erase(0, pos + delimiter.length());
                }
                if (!s.empty())
                {
                    paths.push_back(s);
                }

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
        "native_inject", JSCallbackFuncWithRet([](const ultralight::JSObject &obj,
                                                  const ultralight::JSArgs &args) -> ultralight::JSValue {
            if (args.size() != 5 || !args[0].IsNumber() || !args[1].IsString() || !args[2].IsString() ||
                !args[3].IsBoolean() || !args[4].IsBoolean())
            {
                return ultralight::JSValue("Invalid arguments for injection.");
            }

            DWORD pid = (DWORD)args[0].ToNumber();
            std::string dllPath = ultralight::String(args[1].ToString()).utf8().data();
            std::string method = ultralight::String(args[2].ToString()).utf8().data();
            bool erasePE = args[3].ToBoolean();
            bool hideModule = args[4].ToBoolean();

            std::string result = "";
            if (method == "loadlibrary")
            {
                result = injector::injectLoadLibrary(pid, dllPath);
            }
            else if (method == "apc")
            {
                result = injector::injectApc(pid, dllPath);
            }
            else if (method == "hijack")
            {
                result = injector::injectThreadHijack(pid, dllPath);
            }
            else if (method == "blackbone")
            {
                result = injector::injectBlackBone(pid, dllPath, erasePE, hideModule);
            }
            else if (method == "stealth")
            {
                result = injector::stealthInject(pid, dllPath);
            }
            else
            {
                result = "Unknown injection method specified.";
            }

            return ultralight::JSValue(result.c_str());
        }));

    if (!g_ultralight_controller->IsRendererValid())
    {
        MessageBox(NULL,
                   _T("Failed to initialize Ultralight Renderer!\n\nMake sure the 'resources' and '.dll' files from ")
                   _T("the SDK are in the same directory as your .exe."),
                   _T("Ultralight Error"), MB_OK | MB_ICONERROR);
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
        @import url('https://fonts.googleapis.com/css2?family=Roboto+Mono:wght@400;500&family=Inter:wght@400;500;600;700&display=swap');
        body { font-family: 'Inter', sans-serif; background: transparent; overflow: hidden; }
        .font-mono { font-family: 'Roboto Mono', monospace; }
        .main-window { background-color: #0a0a0a; border: 1px solid #27272a; animation: appear 0.4s ease-out; }
        @keyframes appear { from { opacity: 0; } to { opacity: 1; } }
        ::-webkit-scrollbar { width: 8px; }
        ::-webkit-scrollbar-track { background: #18181b; }
        ::-webkit-scrollbar-thumb { background: #3f3f46; border: 2px solid #18181b; }
        ::-webkit-scrollbar-thumb:hover { background: #52525b; }
        
        .btn { background-color: #27272a; border: 1px solid #3f3f46; color: #d4d4d8; transition: all 0.2s ease; display: flex; align-items: center; justify-content: center; gap: 0.5rem; }
        .btn:hover { border-color: #06b6d4; color: #ffffff; background-color: #3f3f46; }
        .btn-primary { background-color: #0891b2; border-color: #0e7490; color: #ffffff; }
        .btn-primary:hover { background-color: #06b6d4; border-color: #22d3ee; }

        .list-item { display: flex; justify-content: space-between; align-items: center; padding: 0.25rem 0.5rem; cursor: pointer; transition: all 0.15s ease-out; }
        .list-item:hover { background-color: #27272a; }
        .list-item.selected { background-color: rgba(6, 182, 212, 0.1); color: #67e8f9; border: 1px solid rgba(6, 182, 212, 0.2); }
        
        .tooltip {
            position: relative;
            display: inline-block;
        }

        .tooltip .tooltiptext {
            visibility: hidden;
            width: 160px;
            background-color: #18181b;
            color: #d4d4d8;
            text-align: center;
            border-radius: 6px;
            padding: 5px;
            position: absolute;
            z-index: 1;
            bottom: 125%;
            left: 50%;
            margin-left: -80px;
            opacity: 0;
            transition: opacity 0.3s;
            font-size: 12px;
            border: 1px solid #3f3f46;
        }

        .tooltip:hover .tooltiptext {
            visibility: visible;
            opacity: 1;
        }
    </style>
</head>
<body class="flex items-center justify-center min-h-screen">
    <div class="main-window w-full max-w-5xl h-[560px] flex flex-col text-zinc-300 rounded-lg shadow-2xl">
        <header class="flex items-center justify-between p-2 border-b border-zinc-800 flex-shrink-0">
            <div class="flex items-center gap-2">
                <i class="ph-bold ph-planet text-cyan-400 text-lg"></i>
                <h1 class="font-bold tracking-widest text-white">MOON</h1>
            </div>
            <div class="flex items-center gap-2">
                <span class="font-mono text-xs text-zinc-500">v4.2</span>
                <button id="quit-btn" class="text-zinc-500 hover:text-red-500 transition"><i class="ph ph-power text-lg"></i></button>
            </div>
        </header>

        <div class="flex-grow grid grid-cols-12 gap-2 p-2 overflow-hidden">
            
            <div class="col-span-3 flex flex-col border border-zinc-800 p-2 rounded-md">
                <h2 class="text-sm font-semibold text-zinc-400 mb-2 flex-shrink-0 flex items-center justify-between">
                    <span class="flex items-center gap-2"><i class="ph ph-stack"></i>WORKSPACES</span>
                    <div class="flex items-center gap-2">
                        <button id="refresh-workspaces-btn" class="text-zinc-500 hover:text-cyan-400 transition"><i class="ph ph-arrows-clockwise text-lg"></i></button>
                        <button id="add-workspace-btn" class="text-zinc-500 hover:text-cyan-400 transition"><i class="ph ph-plus-circle text-lg"></i></button>
                    </div>
                </h2>
                <div id="workspace-list" class="flex-grow overflow-y-auto border border-zinc-800 bg-black/50 p-1 font-mono text-xs space-y-1 rounded-sm">
                </div>
                <div class="grid grid-cols-2 gap-2 pt-2 flex-shrink-0">
                    <button id="rename-workspace-btn" class="btn text-sm py-1 rounded-md"><i class="ph ph-pencil-simple"></i>Rename</button>
                    <button id="delete-workspace-btn" class="btn text-sm py-1 rounded-md"><i class="ph ph-trash"></i>Delete</button>
                </div>
            </div>

            <div class="col-span-5 flex flex-col border border-zinc-800 p-2 rounded-md min-h-0">
                <div class="flex items-center justify-between mb-2 flex-shrink-0">
                    <label class="text-sm font-semibold text-zinc-400 flex items-center gap-2"><i class="ph ph-cpu"></i>PROCESSES</label>
                    <div class="flex items-center gap-2">
                        <input type="search" id="proc-filter" placeholder="Filter..." class="w-32 bg-zinc-900 border border-zinc-700 px-2 py-1 text-xs rounded-sm focus:outline-none focus:border-cyan-500 transition">
                        <button id="refresh-procs" class="text-zinc-500 hover:text-cyan-400 transition"><i class="ph ph-arrows-clockwise text-lg"></i></button>
                    </div>
                </div>
                <div id="proc-list" class="flex-grow overflow-y-auto min-h-0 border border-zinc-800 bg-black/50 p-1 font-mono text-xs rounded-sm">
                </div>
                 <div class="flex-shrink-0 border border-zinc-800 p-2 rounded-md mt-2">
                    <h2 class="text-sm font-semibold text-zinc-400 mb-2 flex-shrink-0 flex items-center gap-2"><i class="ph ph-terminal-window"></i>STATUS</h2>
                    <div id="status-log" class="h-24 overflow-y-auto bg-black/50 p-2 font-mono text-xs space-y-1 text-zinc-500 rounded-sm">
                    </div>
                </div>
            </div>

            <div class="col-span-4 flex flex-col border border-zinc-800 p-2 rounded-md">
                <div class="flex items-center justify-between mb-2 flex-shrink-0">
                    <h2 id="dll-list-header" class="text-sm font-semibold text-zinc-400 flex items-center gap-2"><i class="ph ph-file-code"></i>INJECTION LIST</h2>
                    <button id="save-changes-btn" class="btn text-xs py-1 px-2 rounded-md"><i class="ph ph-floppy-disk"></i>Save Changes</button>
                </div>
                <div id="dll-list" class="flex-grow overflow-y-auto border border-zinc-800 bg-black/50 p-1 font-mono text-sm space-y-1 rounded-sm">
                </div>
                <div class="grid grid-cols-2 gap-2 pt-2 flex-shrink-0">
                    <button id="add-dll-btn" class="btn text-sm py-1 rounded-md"><i class="ph ph-plus"></i>Add</button>
                    <button id="clear-dlls-btn" class="btn text-sm py-1 rounded-md"><i class="ph ph-trash"></i>Clear</button>
                </div>
            </div>
        </div>
        
        <footer class="flex items-stretch border-t border-zinc-800 flex-shrink-0">
            <div class="w-3/5 p-2 flex flex-col justify-center border-r border-zinc-800">
                <div class="flex justify-between items-center mb-1">
                    <label class="text-xs text-zinc-500">Injection Method</label>
                    <div class="flex items-center gap-2">
                        <input type="checkbox" id="auto-inject-check" class="bg-zinc-900 border-zinc-700 rounded-sm text-cyan-500 focus:ring-0 focus:ring-offset-0">
                        <label for="auto-inject-check" class="text-xs text-zinc-500 select-none">Auto Inject</label>
                    </div>
                </div>
                <div class="relative w-full" id="custom-select">
                    <button id="select-button" class="w-full bg-zinc-800 border border-zinc-700 p-1 text-sm text-left flex justify-between items-center rounded-sm focus:outline-none focus:border-cyan-500 transition">
                        <span id="selected-value">LoadLibrary (Standard)</span>
                        <i class="ph ph-caret-down text-zinc-500"></i>
                    </button>
                    <div id="options-panel" class="absolute bottom-full mb-1 w-full bg-zinc-800 border border-zinc-700 rounded-sm z-10 hidden text-sm">
                        <div class="option p-1 hover:bg-zinc-700 cursor-pointer" data-value="loadlibrary">LoadLibrary (Standard)</div>
                        <div class="option p-1 hover:bg-zinc-700 cursor-pointer" data-value="apc">APC Injection (Stealth)</div>
                        <div class="option p-1 hover:bg-zinc-700 cursor-pointer" data-value="hijack">Thread Hijack (Stealth)</div>
                        <div class="option p-1 hover:bg-zinc-700 cursor-pointer" data-value="blackbone">BlackBone (Manual Map)</div>
                        <div class="option p-1 hover:bg-zinc-700 cursor-pointer" data-value="stealth">BlackBone (Driver Stealth)</div>
                    </div>
                </div>
                <div id="blackbone-options" class="mt-2 space-x-4 hidden">
                    <div class="tooltip inline-flex items-center gap-2">
                        <input type="checkbox" id="erase-pe-check" class="bg-zinc-900 border-zinc-700 rounded-sm text-cyan-500 focus:ring-0 focus:ring-offset-0">
                        <label for="erase-pe-check" class="text-xs text-zinc-400 select-none">Erase PE</label>
                        <span class="tooltiptext">Wipes PE headers from memory to hinder memory analysis.</span>
                    </div>
                    <div class="tooltip inline-flex items-center gap-2">
                        <input type="checkbox" id="hide-module-check" class="bg-zinc-900 border-zinc-700 rounded-sm text-cyan-500 focus:ring-0 focus:ring-offset-0">
                        <label for="hide-module-check" class="text-xs text-zinc-400 select-none">Unlinks the module from PEB to hide it from module lists.</span>
                    </div>
                </div>
            </div>
            <div class="w-2/5 p-2">
                <button id="inject-btn" class="btn-primary w-full h-full text-base font-bold tracking-widest rounded-md flex items-center justify-center gap-2"><i class="ph-bold ph-rocket-launch"></i>INJECT</button>
            </div>
        </footer>
    </div>

    <div id="workspace-modal-overlay" class="fixed inset-0 bg-black/50 z-40 hidden items-center justify-center">
        <div id="workspace-modal" class="bg-zinc-900 border border-zinc-700 p-4 rounded-lg shadow-lg w-96">
            <h3 id="modal-title" class="text-lg font-bold text-white mb-4">Create Workspace</h3>
            <input type="text" id="workspace-name-input" class="w-full bg-zinc-800 border border-zinc-700 p-2 text-sm rounded-sm focus:outline-none focus:border-cyan-500 transition mb-4" placeholder="Workspace name...">
            <div class="flex justify-end gap-2">
                <button id="modal-cancel-btn" class="btn text-sm py-1 px-4 rounded-md">Cancel</button>
                <button id="modal-save-btn" class="btn-primary text-sm py-1 px-4 rounded-md">Save</button>
            </div>
        </div>
    </div>
)HTML_PART1";

    html_content += R"HTML_PART2(
    <script>
        let selectedPid = null;
        let selectedProcName = '';
        let dlls = [];
        let workspaces = [];
        let selectedWorkspaceId = null;
        let injectMethod = 'loadlibrary';

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
        
        const workspaceModalOverlay = document.getElementById('workspace-modal-overlay');
        const modalTitle = document.getElementById('modal-title');
        const workspaceNameInput = document.getElementById('workspace-name-input');
        const modalCancelBtn = document.getElementById('modal-cancel-btn');
        const modalSaveBtn = document.getElementById('modal-save-btn');
        let modalMode = 'add';
        let renameId = null;

        function logStatus(message, type = 'info') {
            const p = document.createElement('p');
            let colorClass = 'text-zinc-500';
            if (type === 'success') colorClass = 'text-green-400';
            if (type === 'error') colorClass = 'text-red-400';
            if (type === 'selection') colorClass = 'text-cyan-400';
            p.innerHTML = `> <span class="${colorClass}">${message}</span>`;
            statusLogDiv.appendChild(p);
            statusLogDiv.scrollTop = statusLogDiv.scrollHeight;
        }
        
        async function initializeApp() {
            logStatus('Welcome to Moon Injector. Ready for action.');
            refreshProcesses();
            await loadWorkspaces();
        }

        async function loadWorkspaces() {
            logStatus('Loading workspaces from database...', 'info');
            const workspacesJson = await window.native_getWorkspaces();
            try {
                workspaces = JSON.parse(workspacesJson);
            } catch (e) {
                logStatus('Error loading workspaces.', 'error');
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
                dllListHeader.innerHTML = `<i class="ph ph-file-code"></i>INJECTION LIST`;
            }
        }

        function renderWorkspaces() {
            workspaceListDiv.innerHTML = '';
            workspaces.forEach(ws => {
                const item = document.createElement('div');
                item.className = 'list-item rounded-sm';
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
            logStatus(`Selected workspace: ${name}`, 'selection');
            dllListHeader.innerHTML = `<i class="ph ph-file-code"></i>INJECTION LIST (${name})`;
            renderWorkspaces();
            await loadDllsForWorkspace(id);
        }

        function openWorkspaceModal(mode, id = null, currentName = '') {
            modalMode = mode;
            renameId = id;
            modalTitle.textContent = mode === 'add' ? 'Create Workspace' : 'Rename Workspace';
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
                    logStatus(`Created workspace: ${name}`, 'success');
                    await loadWorkspaces();
                    await selectWorkspace(newId, name);
                } else {
                    logStatus(`Workspace '${name}' already exists.`, 'error');
                }
            } else if (modalMode === 'rename') {
                await window.native_renameWorkspace(renameId, name);
                logStatus(`Renamed workspace to '${name}'`, 'success');
                await loadWorkspaces();
            }
            closeWorkspaceModal();
        }

        async function deleteWorkspace() {
            if (!selectedWorkspaceId) {
                logStatus('No workspace selected to delete.', 'error');
                return;
            }
            if (workspaces.length <= 1) {
                logStatus('Cannot delete the last workspace.', 'error');
                return;
            }
            const currentWs = workspaces.find(w => w.id === selectedWorkspaceId);
            const confirmed = await window.native_messageBoxYesNo("Confirm Deletion", `Are you sure you want to delete workspace '${currentWs.name}' and all its DLLs?`);
            if (confirmed) {
                await window.native_deleteWorkspace(selectedWorkspaceId);
                logStatus(`Deleted workspace: ${currentWs.name}`, 'success');
                selectedWorkspaceId = null;
                await loadWorkspaces();
            }
        }

        async function refreshProcesses() {
            logStatus('Refreshing process list...');
            const procsJson = await window.native_getProcesses();
            let processes = [];
            try {
                processes = JSON.parse(procsJson);
            } catch (e) {
                logStatus('Error parsing process list.', 'error');
            }
            procListDiv.innerHTML = '';
            processes.sort((a,b) => a.name.localeCompare(b.name)).forEach(proc => {
                const item = document.createElement('div');
                item.className = 'list-item rounded-sm';
                item.innerHTML = `<span>${proc.name}</span><span class="text-zinc-500">PID: ${proc.pid} | ${proc.arch}</span>`;
                item.onclick = () => selectProcess(item, proc.pid, proc.name);
                procListDiv.appendChild(item);
            });
            logStatus(`Found ${processes.length} processes.`, 'success');
        }

        function selectProcess(element, pid, name) {
            const currentSelected = procListDiv.querySelector('.selected');
            if(currentSelected) currentSelected.classList.remove('selected');
            element.classList.add('selected');
            selectedPid = pid;
            selectedProcName = name;
            logStatus(`Process selected: ${name} (PID: ${pid})`, 'selection');
            if (autoInjectCheckbox.checked) {
                logStatus('Auto-inject triggered!', 'selection');
                doInject();
            }
        }
        
        function filterProcesses() {
            const filterText = document.getElementById('proc-filter').value.toLowerCase();
            const items = procListDiv.getElementsByClassName('list-item');
            for(let i=0; i < items.length; i++) {
                const name = items[i].querySelector('span:first-child').textContent.toLowerCase();
                if (name.includes(filterText)) {
                    items[i].style.display = 'flex';
                } else {
                    items[i].style.display = 'none';
                }
            }
        }

        async function addDll() {
            const path = await window.native_openDllDialog();
            if (path) {
                addDllToList(path);
                logStatus('DLL added. Click "Save Changes" to persist.', 'info');
            }
        }

        function addDllToList(path) {
            const dllName = path.substring(path.lastIndexOf('\\') + 1);
            if (dlls.some(d => d.path === path)) { return; }
            dlls.push({ path: path, name: dllName, selected: true });
            renderDlls();
        }
        
        async function loadDllsForWorkspace(workspaceId) {
            dlls = [];
            const dllsJson = await window.native_getDlls(workspaceId);
            let savedDlls = [];
            try {
                 savedDlls = JSON.parse(dllsJson);
            } catch(e) {
                logStatus('Error loading DLL list.', 'error');
            }
            savedDlls.forEach(path => addDllToList(path));
            logStatus(`Loaded ${dlls.length} DLLs for workspace.`, 'info');
            renderDlls();
        }
        
        function renderDlls() {
            dllListDiv.innerHTML = '';
            dlls.forEach((dll, index) => {
                const item = document.createElement('div');
                item.className = 'list-item group rounded-sm';
                if(dll.selected) item.classList.add('selected');
                const icon = dll.selected ? 'ph-fill ph-check-square text-cyan-400' : 'ph ph-square';
                item.innerHTML = `
                    <span class="flex items-center gap-2" onclick="toggleDll(${index})">
                        <i class="${icon}"></i>${dll.name}
                    </span>
                    <div class="text-zinc-600 group-hover:text-zinc-400 transition flex gap-2">
                        <i class="ph ph-x cursor-pointer" onclick="removeDll(${index})"></i>
                    </div>`;
                dllListDiv.appendChild(item);
            });
            if (dlls.length === 0) {
                 dllListDiv.innerHTML = '<div class="text-center text-zinc-600 text-xs p-4">No DLLs in this workspace.</div>';
            }
        }

        function toggleDll(index) {
            dlls[index].selected = !dlls[index].selected;
            renderDlls();
        }

        function removeDll(index) {
            const removed = dlls.splice(index, 1);
            if (removed.length > 0) {
                logStatus(`Removed ${removed[0].name}. Click "Save Changes".`, 'info');
                renderDlls();
            }
        }

        function clearDlls() {
            if (dlls.length === 0) return;
            dlls = [];
            logStatus('Cleared DLL list. Click "Save Changes".', 'info');
            renderDlls();
        }
        
        async function saveChanges() {
            if (!selectedWorkspaceId) {
                logStatus('Cannot save. No workspace selected.', 'error');
                return;
            }
            logStatus(`Saving DLL list for workspace...`, 'info');
            const paths = dlls.map(d => d.path);
            const pathsStr = paths.join('_|_');
            await window.native_saveDlls(selectedWorkspaceId, pathsStr);
            logStatus('Workspace saved successfully.', 'success');
        }

        async function doInject() {
            if(!selectedPid) {
                logStatus('No process selected!', 'error');
                return;
            }
            const dllsToInject = dlls.filter(d => d.selected);
            if(dllsToInject.length === 0) {
                logStatus('No DLLs selected for injection!', 'error');
                return;
            }
            const method = injectMethod;
            const erasePE = erasePECheckbox.checked;
            const hideModule = hideModuleCheckbox.checked;
            logStatus(`Starting injection into ${selectedProcName} (PID: ${selectedPid})...`);
            logStatus(`Method: ${selectedValueSpan.textContent}`);
            for (const dll of dllsToInject) {
                const dllName = dll.name;
                logStatus(`Injecting ${dllName}...`);
                const result = await window.native_inject(selectedPid, dll.path, method, erasePE, hideModule);
                if (result.toLowerCase().includes('success')) {
                    logStatus(` > ${dllName}: ${result}`, 'success');
                } else {
                    logStatus(` > ${dllName}: ${result}`, 'error');
                }
            }
        }

        selectButton.onclick = () => optionsPanel.classList.toggle('hidden');
        document.querySelectorAll('.option').forEach(option => {
            option.onclick = () => {
                injectMethod = option.getAttribute('data-value');
                selectedValueSpan.textContent = option.textContent;
                optionsPanel.classList.add('hidden');
                blackboneOptionsDiv.classList.toggle('hidden', injectMethod !== 'blackbone');
                blackboneOptionsDiv.classList.toggle('flex', injectMethod === 'blackbone');
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
                logStatus('No workspace selected to rename.', 'error');
                return;
            }
            const currentWs = workspaces.find(w => w.id === selectedWorkspaceId);
            openWorkspaceModal('rename', selectedWorkspaceId, currentWs.name);
        };
        document.getElementById('delete-workspace-btn').onclick = deleteWorkspace;
        document.getElementById('add-dll-btn').onclick = addDll;
        document.getElementById('clear-dlls-btn').onclick = clearDlls;
        document.getElementById('save-changes-btn').onclick = saveChanges;
        document.getElementById('inject-btn').onclick = doInject;
        document.getElementById('quit-btn').onclick = () => window.native_quit();
        modalCancelBtn.onclick = closeWorkspaceModal;
        modalSaveBtn.onclick = saveWorkspace;
        workspaceNameInput.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') saveWorkspace();
            if (e.key === 'Escape') closeWorkspaceModal();
        });

    </script>
</body>
</html>
)HTML_PART2";

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
