#pragma once
#include <windows.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")

// 入力
class Input
{
public: // メンバ変数
	// 初期化
	void Initialize(HINSTANCE hInstance,HWND hwnd);
	// 更新
	void Update();

private: // メンバ変数
	// キーボードのデバイス
	IDirectInputDevice8 *keyboard = nullptr;
};

