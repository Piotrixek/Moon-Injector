#include <Windows.h>
#include <tchar.h>
#include <shellapi.h>
#include <gdiplus.h>
#include <objidl.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "dx_setup.h"
#include "ui_style.h"
#include "window_setup.h"

#include "font.h"
#include "icons.h"
#include "ultralight_controller.h"
#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <map>
#include "menu.h"
#include "injector.h"
#include "tinyfiledialogs.h"

std::unique_ptr<UltralightController> g_ultralight_controller;


int main(int, char**)
{
    HINSTANCE hInstance = GetModuleHandle(NULL);
    const TCHAR* className = _T("ImGuiAppClass");
    HWND hwnd = SetupWindow(hInstance, className);
    if (!hwnd) {
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
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    ImFontConfig main_font_config;
    main_font_config.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryTTF((void*)my_Font, sizeof(my_Font), 16.0f, &main_font_config);

    static const ImWchar icons_ranges[] = { 0xf000, 0xf3ff, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.OversampleH = 3;
    icons_config.OversampleV = 3;
    icons_config.EllipsisChar = 0xf141;
    io.Fonts->AddFontFromMemoryCompressedTTF(font_awesome_data, font_awesome_size, 18.5f, &icons_config, icons_ranges);

    ApplyCommandMenuStyle();

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(GetDevice(), GetImmediateContext());

    try {
        g_ultralight_controller = std::make_unique<UltralightController>(GetDevice(), GetImmediateContext());
    }
    catch (const std::exception& e) {
        std::string error_msg = "An exception occurred during Ultralight initialization:\n\n";
        error_msg += e.what();
        error_msg += "\n\nThis usually means the 'resources' folder is missing from the executable's directory.";
        MessageBoxA(NULL, error_msg.c_str(), "Ultralight Initialization Failed", MB_OK | MB_ICONERROR);
        return 1;
    }

    g_ultralight_controller->AddCallback("native_getProcesses",
        JSCallbackFuncWithRet([](const ultralight::JSObject& obj, const ultralight::JSArgs& args) -> ultralight::JSValue {
            auto procs = injector::getProcs();
            std::stringstream ss;
            ss << "[";
            for (size_t i = 0; i < procs.size(); ++i) {
                ss << "{\"pid\":" << procs[i].pid << ", \"name\":\"" << procs[i].name << "\", \"arch\":\"" << procs[i].arch << "\"}";
                if (i < procs.size() - 1) ss << ",";
            }
            ss << "]";
            return ultralight::JSValue(ss.str().c_str());
            })
    );

    g_ultralight_controller->AddCallback("native_addDll",
        JSCallbackFuncWithRet([](const ultralight::JSObject& obj, const ultralight::JSArgs& args) -> ultralight::JSValue {
            const char* filterPatterns[1] = { "*.dll" };
            const char* filePath = tinyfd_openFileDialog("Select DLL", "", 1, filterPatterns, "DLL Files", 0);
            if (filePath) {
                return ultralight::JSValue(filePath);
            }
           // return ultralight::JSValue(ultralight::JSValue::Type::Null);
            })
    );

    g_ultralight_controller->AddCallback("native_inject",
        JSCallbackFuncWithRet([](const ultralight::JSObject& obj, const ultralight::JSArgs& args) -> ultralight::JSValue {
            if (args.size() != 5 || !args[0].IsNumber() || !args[1].IsString() || !args[2].IsString() || !args[3].IsBoolean() || !args[4].IsBoolean()) {
                return ultralight::JSValue("Invalid arguments for injection.");
            }

            DWORD pid = (DWORD)args[0].ToNumber();
            std::string dllPath = ultralight::String(args[1].ToString()).utf8().data();
            std::string method = ultralight::String(args[2].ToString()).utf8().data();
            bool erasePE = args[3].ToBoolean();
            bool hideModule = args[4].ToBoolean();

            std::string result = "";
            if (method == "loadlibrary") {
                result = injector::injectLoadLibrary(pid, dllPath);
            }
            else if (method == "apc") {
                result = injector::injectApc(pid, dllPath);
            }
            else if (method == "hijack") {
                result = injector::injectThreadHijack(pid, dllPath);
            }
            else if (method == "blackbone") {
                result = injector::injectBlackBone(pid, dllPath, erasePE, hideModule);
            }
            else if (method == "stealth") {
                result = injector::stealthInject(pid, dllPath);
            }
            else {
                result = "Unknown injection method specified.";
            }

            return ultralight::JSValue(result.c_str());
            })
    );

    if (!g_ultralight_controller->IsRendererValid()) {
        MessageBox(NULL, _T("Failed to initialize Ultralight Renderer!\n\nMake sure the 'resources' and '.dll' files from the SDK are in the same directory as your .exe."), _T("Ultralight Error"), MB_OK | MB_ICONERROR);
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
        body { font-family: 'Inter', sans-serif; background: transparent; }
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
        .custom-select {
            background-image: url("data:image/svg+xml,%3csvg xmlns='http://www.w3.org/2000/svg' fill='none' viewBox='0 0 20 20'%3e%3cpath stroke='%236b7280' stroke-linecap='round' stroke-linejoin='round' stroke-width='1.5' d='M6 8l4 4 4-4'/%3e%3c/svg%3e");
            background-position: right 0.5rem center;
            background-repeat: no-repeat;
            background-size: 1.5em 1.5em;
        }
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
    <div class="main-window w-full max-w-4xl h-[560px] flex flex-col text-zinc-300 rounded-lg shadow-2xl">
        <header class="flex items-center justify-between p-2 border-b border-zinc-800 flex-shrink-0">
            <div class="flex items-center gap-2">
                <i class="ph-bold ph-planet text-cyan-400 text-lg"></i>
                <h1 class="font-bold tracking-widest text-white">MOON</h1>
            </div>
            <span class="font-mono text-xs text-zinc-500">v4.0</span>
        </header>

        <div class="flex-grow grid grid-cols-5 grid-rows-3 gap-2 p-2 overflow-hidden">
            
            <div class="col-span-3 row-span-2 flex flex-col border border-zinc-800 p-2 rounded-md min-h-0">
                <div class="flex items-center justify-between mb-2 flex-shrink-0">
                    <label class="text-sm font-semibold text-zinc-400 flex items-center gap-2"><i class="ph ph-cpu"></i>PROCESSES</label>
                    <div class="flex items-center gap-2">
                        <input type="search" id="proc-filter" placeholder="Filter..." class="w-32 bg-zinc-900 border border-zinc-700 px-2 py-1 text-xs rounded-sm focus:outline-none focus:border-cyan-500 transition">
                        <button id="refresh-procs" class="text-zinc-500 hover:text-cyan-400 transition"><i class="ph ph-arrows-clockwise text-lg"></i></button>
                    </div>
                </div>
                <div id="proc-list" class="flex-grow overflow-y-auto min-h-0 border border-zinc-800 bg-black/50 p-1 font-mono text-xs rounded-sm">
                </div>
            </div>

            <div class="col-span-2 row-span-3 flex flex-col border border-zinc-800 p-2 rounded-md">
                <h2 class="text-sm font-semibold text-zinc-400 mb-2 flex-shrink-0 flex items-center gap-2"><i class="ph ph-file-code"></i>INJECTION LIST</h2>
                <div id="dll-list" class="flex-grow overflow-y-auto border border-zinc-800 bg-black/50 p-1 font-mono text-sm space-y-1 rounded-sm">
                </div>
                <div class="grid grid-cols-2 gap-2 pt-2 flex-shrink-0">
                    <button id="add-dll" class="btn text-sm py-1 rounded-md"><i class="ph ph-plus"></i>Add</button>
                    <button id="clear-dlls" class="btn text-sm py-1 rounded-md"><i class="ph ph-trash"></i>Clear</button>
                </div>
            </div>

            <div class="col-span-3 row-span-1 flex flex-col border border-zinc-800 p-2 rounded-md">
                <h2 class="text-sm font-semibold text-zinc-400 mb-2 flex-shrink-0 flex items-center gap-2"><i class="ph ph-terminal-window"></i>STATUS</h2>
                <div id="status-log" class="flex-grow overflow-y-auto border border-zinc-800 bg-black/50 p-2 font-mono text-xs space-y-1 text-zinc-500 rounded-sm">
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
                <select id="inject-method" class="w-full bg-zinc-800 border border-zinc-700 p-1 text-sm focus:outline-none focus:border-cyan-500 transition appearance-none custom-select rounded-sm">
                    <option value="loadlibrary">LoadLibrary (Standard)</option>
                    <option value="apc">APC Injection (Stealth)</option>
                    <option value="hijack">Thread Hijack (Stealth)</option>
                    <option value="blackbone">BlackBone (Manual Map)</option>
                    <option value="stealth">BlackBone (Driver Stealth)</option>
                </select>
                <div id="blackbone-options" class="mt-2 space-x-4 hidden">
                    <div class="tooltip inline-flex items-center gap-2">
                        <input type="checkbox" id="erase-pe-check" class="bg-zinc-900 border-zinc-700 rounded-sm text-cyan-500 focus:ring-0 focus:ring-offset-0">
                        <label for="erase-pe-check" class="text-xs text-zinc-400 select-none">Erase PE</label>
                        <span class="tooltiptext">Wipes PE headers from memory to hinder memory analysis.</span>
                    </div>
                    <div class="tooltip inline-flex items-center gap-2">
                        <input type="checkbox" id="hide-module-check" class="bg-zinc-900 border-zinc-700 rounded-sm text-cyan-500 focus:ring-0 focus:ring-offset-0">
                        <label for="hide-module-check" class="text-xs text-zinc-400 select-none">Hide Module</label>
                        <span class="tooltiptext">Unlinks the module from PEB to hide it from module lists.</span>
                    </div>
                </div>
            </div>
            <div class="w-2/5 p-2">
                <button id="inject-btn" class="btn-primary w-full h-full text-base font-bold tracking-widest rounded-md flex items-center justify-center gap-2"><i class="ph-bold ph-rocket-launch"></i>INJECT</button>
            </div>
        </footer>
    </div>
)HTML_PART1";

    html_content += R"HTML_PART2(
    <script>
        let selectedPid = null;
        let selectedProcName = '';
        let dlls = []; // this will hold { path, name, selected }

        const procListDiv = document.getElementById('proc-list');
        const dllListDiv = document.getElementById('dll-list');
        const statusLogDiv = document.getElementById('status-log');
        const injectMethodSelect = document.getElementById('inject-method');
        const blackboneOptionsDiv = document.getElementById('blackbone-options');
        const erasePECheckbox = document.getElementById('erase-pe-check');
        const hideModuleCheckbox = document.getElementById('hide-module-check');
        const autoInjectCheckbox = document.getElementById('auto-inject-check');

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
        
        // --- Process Functions ---
        async function refreshProcesses() {
            logStatus('Refreshing process list...');
            const procsJson = await window.native_getProcesses();
            const processes = JSON.parse(procsJson);
            procListDiv.innerHTML = ''; // clear old list
            
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

        // --- DLL Functions ---
        async function addDll() {
            const path = await window.native_addDll();
            if (path) {
                const dllName = path.substring(path.lastIndexOf('\\') + 1);
                if (dlls.some(d => d.path === path)) {
                    logStatus(`DLL already in list: ${dllName}`, 'error');
                    return;
                }
                dlls.push({ path: path, name: dllName, selected: true });
                logStatus(`Added DLL: ${dllName}`, 'success');
                renderDlls();
            }
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
            const selectedCount = dlls.filter(d => d.selected).length;
            if (dlls.length > 0) {
                 logStatus(`${selectedCount} DLL(s) selected for injection.`, 'selection');
            }
        }

        function toggleDll(index) {
            dlls[index].selected = !dlls[index].selected;
            renderDlls();
        }

        function removeDll(index) {
            const removed = dlls.splice(index, 1);
            logStatus(`Removed DLL: ${removed[0].name}`);
            renderDlls();
        }

        function clearDlls() {
            dlls = [];
            logStatus('Cleared all DLLs.');
            renderDlls();
        }

        // --- Injection Functions ---
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

            const method = injectMethodSelect.value;
            const erasePE = erasePECheckbox.checked;
            const hideModule = hideModuleCheckbox.checked;

            logStatus(`Starting injection into ${selectedProcName} (PID: ${selectedPid})...`);
            logStatus(`Method: ${injectMethodSelect.options[injectMethodSelect.selectedIndex].text}`);

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

        // --- Event Listeners ---
        document.addEventListener('DOMContentLoaded', () => {
            logStatus('Welcome to Moon Injector. Ready for action.');
            refreshProcesses();
        });

        injectMethodSelect.onchange = () => {
            if (injectMethodSelect.value === 'blackbone') {
                blackboneOptionsDiv.classList.remove('hidden');
                blackboneOptionsDiv.classList.add('flex');
            } else {
                blackboneOptionsDiv.classList.add('hidden');
                blackboneOptionsDiv.classList.remove('flex');
            }
        };

        document.getElementById('refresh-procs').onclick = refreshProcesses;
        document.getElementById('proc-filter').onkeyup = filterProcesses;
        document.getElementById('add-dll').onclick = addDll;
        document.getElementById('clear-dlls').onclick = clearDlls;
        document.getElementById('inject-btn').onclick = doInject;

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

        const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        ID3D11RenderTargetView* mainRenderTargetView = GetMainRenderTargetView();
        ID3D11DeviceContext* context = GetImmediateContext();

        if (mainRenderTargetView && context) {
            context->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
            context->ClearRenderTargetView(mainRenderTargetView, clear_color_with_alpha);
        }

        ImGui::Render();
        if (ImGui::GetDrawData()) {
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        IDXGISwapChain* swapChain = GetSwapChain();
        if (swapChain) {
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
