#pragma once
#include <windows.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")

#include "WinApp.h"

// 入力
class Input
{
public: // メンバ変数
	// 初期化
	void Initialize(WinApp* winApp);
	// 更新
	void Update();

	bool PushKey(BYTE keyNumber);

private: // メンバ変数
	// キーボードのデバイス
	IDirectInputDevice8 *keyboard = nullptr;

	BYTE key[256] = {};

	// WindowsAPI
	WinApp *winApp_ = nullptr;
};

