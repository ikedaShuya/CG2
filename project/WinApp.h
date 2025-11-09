#pragma once
#include <windows.h>

// WindowsAPI
class WinApp
{
public: // 性的メンバ変数
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

public: // メンバ変数
	// 初期化
	void Initialize();
	// 更新
	void Update();
};

