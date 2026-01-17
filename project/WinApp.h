#pragma once
#include <Windows.h>
#include <cstdint>

// WindowsAPI
class WinApp
{
public: // 性的メンバ変数
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

public: // 定数
	// クライアント領域のサイズ
	static const int32_t kClientWidth = 1280;
	static const int32_t kClientHeight = 720;

public: // メンバ変数
	// 初期化
	void Initialize();
	// 更新
	void Update();

	// getter
	HWND GetHwnd() const { return hwnd; }

	// getter
	HINSTANCE GetHInstance() const { return wc.hInstance; }

	// 終了
	void Finalize();

	// メッセージの処理
	bool ProcessMessage();

private:
	// ウィンドウハンドル
	HWND hwnd = nullptr;

	// ウィンドウクラスの設定
	WNDCLASS wc {};
};