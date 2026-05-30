#include "window_setup.h"
#include "dx_setup.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include <comdef.h>
#include <memory>
#include <tchar.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

HWND SetupWindow(HINSTANCE hInstance, const TCHAR *className)
{
    WNDCLASSEX wc = {sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, hInstance, NULL, NULL, NULL, NULL,
                     className,          NULL};
    if (!::RegisterClassEx(&wc))
    {
        MessageBox(NULL, _T("Failed to register window class!"), _T("Error"), MB_OK | MB_ICONERROR);
        return NULL;
    }

    HWND hwnd = ::CreateWindowEx(0, wc.lpszClassName, _T("V-Launch Host"), WS_POPUP, 0, 0, 1, 1, NULL, NULL,
                                 wc.hInstance, NULL);

    if (!hwnd)
    {
        MessageBox(NULL, _T("Failed to create hidden host window!"), _T("Error"), MB_OK | MB_ICONERROR);
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return NULL;
    }

    return hwnd;
}

void CleanupWindow(HINSTANCE hInstance, const TCHAR *className)
{
    ::UnregisterClass(className, hInstance);
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE: {
        IDXGISwapChain *swapChain = GetSwapChain();
        if (swapChain != NULL && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            HRESULT hr =
                swapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            if (SUCCEEDED(hr))
            {
                CreateRenderTarget();
            }
            else
            {
                _com_error err(hr);
                MessageBox(hWnd, err.ErrorMessage(), _T("ResizeBuffers Failed"), MB_OK | MB_ICONERROR);
            }
        }
        return 0;
    }
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }

    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
