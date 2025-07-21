#pragma once
#include <Windows.h>
#include <tchar.h>

HWND SetupWindow(HINSTANCE hInstance, const TCHAR* className);
void CleanupWindow(HINSTANCE hInstance, const TCHAR* className);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);