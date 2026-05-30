#include "dx_setup.h"
#include <comdef.h>
#include <tchar.h>

static ID3D11Device *g_pd3dDevice = nullptr;
static ID3D11DeviceContext *g_pd3dDeviceContext = nullptr;
static IDXGISwapChain *g_pSwapChain = nullptr;
static ID3D11RenderTargetView *g_mainRenderTargetView = nullptr;

bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags,
                                               featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain,
                                               &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (FAILED(hr))
    {
        _com_error err(hr);
        MessageBox(hWnd, err.ErrorMessage(), _T("D3D11CreateDeviceAndSwapChain Failed"), MB_OK | MB_ICONERROR);
        CleanupDeviceD3D();
        return false;
    }

    CreateRenderTarget();
    if (!g_mainRenderTargetView)
    {
        CleanupDeviceD3D();
        return false;
    }

    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain)
    {
        g_pSwapChain->Release();
        g_pSwapChain = nullptr;
    }
    if (g_pd3dDeviceContext)
    {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = nullptr;
    }
    if (g_pd3dDevice)
    {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
}

void CreateRenderTarget()
{
    if (!g_pSwapChain || !g_pd3dDevice)
        return;

    if (g_mainRenderTargetView)
    {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }

    ID3D11Texture2D *pBackBuffer = nullptr;
    HRESULT hr = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (SUCCEEDED(hr) && pBackBuffer)
    {
        hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
        pBackBuffer->Release();
        if (FAILED(hr))
        {
            _com_error err(hr);
            MessageBox(NULL, err.ErrorMessage(), _T("CreateRenderTargetView Failed"), MB_OK | MB_ICONERROR);
            g_mainRenderTargetView = nullptr;
        }
    }
    else
    {
        _com_error err(hr);
        MessageBox(NULL, err.ErrorMessage(), _T("GetBuffer Failed"), MB_OK | MB_ICONERROR);
        g_mainRenderTargetView = nullptr;
    }
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView)
    {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

ID3D11Device *GetDevice()
{
    return g_pd3dDevice;
}
ID3D11DeviceContext *GetImmediateContext()
{
    return g_pd3dDeviceContext;
}
IDXGISwapChain *GetSwapChain()
{
    return g_pSwapChain;
}
ID3D11RenderTargetView *GetMainRenderTargetView()
{
    return g_mainRenderTargetView;
}
