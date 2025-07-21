#pragma once

#include <d3d11.h>
#include <Windows.h> 

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();

ID3D11Device* GetDevice();
ID3D11DeviceContext* GetImmediateContext();
IDXGISwapChain* GetSwapChain();
ID3D11RenderTargetView* GetMainRenderTargetView();