#include "WinApp.h" 

#include <format>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <numbers>

#include <strsafe.h>
#include <DbgHelp.h>
#include <xaudio2.h>

#include "Input.h"
#include "DirectXCommon.h"
#include "D3DResourceLeakChecker.h"
#include "SpriteCommon.h"
#include "Sprite.h"
#include "MathFunctions.h"
#include "TextureManager.h"
#include "Object3dCommon.h"
#include "Object3d.h"
#include "Model.h"
#include "ModelCommon.h"
#include "ModelManager.h"
#include "Skybox.h"
#include "SkyboxCommon.h"

#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"dxcompiler.lib")
#pragma comment(lib,"xaudio2.lib")
#pragma comment(lib,"Dbghelp.lib")

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

using namespace math;

// チャンクヘッダ
struct ChunkHeader
{
	char id[4]; // チャンク毎のID
	int32_t size; // チャンクサイズ
};

// RIFFヘッダチャンク
struct RiffHeader
{
	ChunkHeader chunk; // "RIFF"
	char type[4]; // "WAVE"
};

// FMTチャンク
struct FormatChunk
{
	ChunkHeader chunk; // "fmt"
	WAVEFORMATEX fmt; // 波形フォーマット
};

// 音声データ
struct SoundData
{
	// 波形フォーマット
	WAVEFORMATEX wfex;
	// バッファの先頭アドレス
	BYTE *pBuffer;
	// バッファのサイズ
	unsigned int bufferSize;
};

SoundData SoundLoadWave(const char *filename)
{

	//----ファイルオープン----

	// ファイル入力ストリームのインスタンス
	std::ifstream file;
	// .wavファイルをバイナリモードで開く
	file.open(filename, std::ios_base::binary);
	// ファイルオープン失敗を検出する
	assert(file.is_open());

	//----.wavデータを読み込み----

	// RIFFヘッダーの読み込み
	RiffHeader riff;
	file.read((char *)&riff, sizeof(riff));
	// ファイルがRIFFかチェック
	if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
		assert(0);
	}
	// タイプがWAVEかチェック
	if (strncmp(riff.type, "WAVE", 4) != 0) {
		assert(0);
	}

	// Formatチャンクの読み込み
	FormatChunk format = {};
	// チャンクヘッダーの確認
	file.read((char *)&format, sizeof(ChunkHeader));
	if (strncmp(format.chunk.id, "fmt ", 4) != 0) {
		assert(0);
	}

	// チャンク本体の読み込み
	assert(format.chunk.size <= sizeof(format.fmt));
	file.read((char *)&format.fmt, format.chunk.size);

	// Dataチャンクの読み込み
	ChunkHeader data;
	file.read((char *)&data, sizeof(data));
	// JUNKチャンクを検出した場合
	if (strncmp(data.id, "JUNK", 4) == 0) {
		// 読み取り位置をJUNKチャンクの終わりまで進める
		file.seekg(data.size, std::ios_base::cur);
		// 再読み込み
		file.read((char *)&data, sizeof(data));
	}

	if (strncmp(data.id, "data", 4) != 0) {
		assert(0);
	}

	// Dataチャンクのデータ部（波形データ）の読み込み
	char *pBuffer = new char[data.size];
	file.read(pBuffer, data.size);

	//----ファイルクローズ----

	// Waveファイルを閉じる
	file.close();

	//----読み込んだ音声データをreturn----

	// returnする為の音声データ
	SoundData soundData = {};

	soundData.wfex = format.fmt;
	soundData.pBuffer = reinterpret_cast<BYTE *>(pBuffer);
	soundData.bufferSize = data.size;

	return soundData;
}

// 音声データを解放
void SoundUnload(SoundData *soundData)
{
	// バッファのメモリを解放
	delete[] soundData->pBuffer;

	soundData->pBuffer = 0;
	soundData->bufferSize = 0;
	soundData->wfex = {};
}

// 音楽再生
void SoundPlayWave(IXAudio2 *xAudio2, const SoundData &soundData) {

	HRESULT result;

	// 波形フォーマットを元にSourceVoiceの生成
	IXAudio2SourceVoice *pSourceVoice = nullptr;
	result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
	assert(SUCCEEDED(result));

	// 再生する波形データの設定
	XAUDIO2_BUFFER buf {};
	buf.pAudioData = soundData.pBuffer;
	buf.AudioBytes = soundData.bufferSize;
	buf.Flags = XAUDIO2_END_OF_STREAM;

	// 波形データの再生
	result = pSourceVoice->SubmitSourceBuffer(&buf);
	result = pSourceVoice->Start();
}

#pragma region SystemBase

// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

#ifdef USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
#endif

	// メッセージに応じてゲーム固有の処理を行う
	switch (msg) {
		// ウィンドウが破棄された
		case WM_DESTROY:
		// osに対して、アプリの終了を伝える
		PostQuitMessage(0);
		return 0;
	}

	// 標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

// Dump出力
static LONG WINAPI ExportDump(EXCEPTION_POINTERS *exception) {

	// 時刻を取得して、時刻を名前に入れたファイルを作成。Dumpsディレクトリ以下に出力
	SYSTEMTIME time;
	GetLocalTime(&time);
	wchar_t filePath[MAX_PATH] = { 0 };
	CreateDirectory(L"./Dumps", nullptr);
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
	HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
	// processId（このexeのId）とクラッシュ（例外）の発生したthreadIdを取得
	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();
	// 設定情報を入力
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation { 0 };
	minidumpInformation.ThreadId = threadId;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;
	// Dumpを出力。MiniDumpNormalは最低限の情報を出力するフラグ
	MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);
	// 他に関連づけられているSEH例外ハンドラがあれば実行。通常はプロセスを終了する
	return EXCEPTION_EXECUTE_HANDLER;
}

#pragma endregion

// Windowsアプリでのエントリーポイント（main関数）
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

	// 誰も補捉しなかった場合に(Unhandled)、補捉する関数を登録
	SetUnhandledExceptionFilter(ExportDump);

#pragma region ログファイル

	// ログのディレクトリを用意
	std::filesystem::create_directory("logs");

	// 現在時刻を取得（UTC時刻）
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	// ログファイルの名前にコンマ何秒はいらないので、削って秒にする
	std::chrono::time_point < std::chrono::system_clock, std::chrono::seconds>
		nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
	// 日本時間（PCの設定時間）に変換
	std::chrono::zoned_time localTime { std::chrono::current_zone(),nowSeconds };
	// formatを使って年月日_時分秒の文字列に変換
	std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
	// 時刻を作ってファイル名を決定
	std::string logFilePath = std::string("logs/") + dateString + ".log";
	// ファイルを作って書き込み準備
	std::ofstream logStream(logFilePath);

#pragma endregion

#pragma region 基盤システムの初期化

	// ===== ウィンドウ関連 =====
	WinApp *winApp = new WinApp();
	winApp->Initialize();

	// ===== DirectX関連 =====
	DirectXCommon *dxCommon = new DirectXCommon();
	dxCommon->Initialize(winApp);

	// ===== 3Dオブジェクト共通部初期化 =====
	Object3dCommon *object3dCommon = new Object3dCommon();
	object3dCommon->Initialize(dxCommon);

	// ===== モデル共通部初期化 =====
	ModelCommon *modelCommon = new ModelCommon();
	modelCommon->Initialize(dxCommon);

	// ===== テクスチャ管理 =====
	auto textureManager = TextureManager::GetInstance();
	textureManager->SetDirectXCommon(dxCommon);
	textureManager->Initialize();

	// ===== スプライト共通処理 =====
	SpriteCommon *spriteCommon = new SpriteCommon();
	spriteCommon->Initialize(dxCommon);

	// ===== オーディオ（XAudio2）初期化 =====
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
	IXAudio2MasteringVoice *masterVoice = nullptr;
	HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	result = xAudio2->CreateMasteringVoice(&masterVoice);

	// ===== サウンド再生 =====
	SoundData soundData1 = SoundLoadWave("resources/audio/Alarm02.wav");
	SoundPlayWave(xAudio2.Get(), soundData1);

	// ===== 入力処理 =====
	Input *input = new Input();
	input->Initialize(winApp);

	// ===== 3Dモデルマネージャー初期化 =====
	ModelManager::GetInstance()->Initialize(dxCommon);

#pragma endregion

#pragma region 最初のシーンの初期化

	// ===== モデルのロード =====
	// ModelManager::GetInstance()->LoadModel("resources/models/fence/plane.obj"); 
	// ModelManager::GetInstance()->LoadModel("resources/models/bunny/bunny.obj");
	ModelManager::GetInstance()->LoadModel("resources/models/teapot/teapot.obj");

	// ===== Object3d インスタンス生成・初期化 =====
	// Object3d *planeObject = new Object3d();
	// Object3d *bunnyObject = new Object3d();
	Object3d *fenceObject = new Object3d();

	// planeObject->Initialize(object3dCommon);
	// bunnyObject->Initialize(object3dCommon);
	fenceObject->Initialize(object3dCommon);

	// ===== モデル設定 =====
	// planeObject->SetModel("resources/models/plane/plane.obj");
	// bunnyObject->SetModel("resources/models/bunny/bunny.obj");
	fenceObject->SetModel("resources/models/teapot/teapot.obj");
	fenceObject->SetEnvironmentTexture(
		"resources/rostock_laage_airport_4k.dds"
	);

	// ===== Transform設定 =====
	//// planeObject
	//planeObject->SetTranslate({ -2.42f, -0.43f, 0.0f });
	//planeObject->SetRotate({ 0.0f, 0.0f, 0.0f });
	//planeObject->SetScale({ 1.0f, 1.0f, 1.0f });

	//// bunnyObject
	//bunnyObject->SetTranslate({ 2.6f, -1.46f, 0.0f });
	//bunnyObject->SetRotate({ 0.0f, 0.0f, 0.0f });
	//bunnyObject->SetScale({ 1.0f, 1.0f, 1.0f });

	// fenceObject
	fenceObject->SetTranslate({ 0.0f, 0.0f, 0.0f });
	fenceObject->SetRotate({ 0.0f, 0.0f, 0.0f });
	fenceObject->SetScale({ 1.0f, 1.0f, 1.0f });

	// ===== スプライトの生成と初期化 =====
	Sprite *uvCheckerSprite = new Sprite();
	uvCheckerSprite->Initialize(spriteCommon, "resources/textures/uvChecker.png");

	// ===== スプライト設定 =====
	uvCheckerSprite->SetIsFlipX(false);
	uvCheckerSprite->SetIsFlipY(false);

	uvCheckerSprite->SetTextureLeftTop({ 0.0f, 0.0f });
	uvCheckerSprite->SetTextureSize({ 512.0f, 512.0f });

	uvCheckerSprite->SetSize({ 256.0f, 256.0f });

	// 画面中央に配置
	uvCheckerSprite->SetPosition({ 128.0f, 128.0f });

	// アンカーポイントは中央
	uvCheckerSprite->SetAnchorPoint({ 0.5f, 0.5f });

	// 初期化
	SkyboxCommon* skyboxCommon = new SkyboxCommon;
	skyboxCommon->Initialize(dxCommon);

	Skybox* skybox = new Skybox;
	skybox->Initialize(skyboxCommon);

	// Cubemapを設定
	skybox->SetTexture("resources/rostock_laage_airport_4k.dds");

#pragma endregion

	D3DResourceLeakChecker leakCheck;

#pragma region メインループ

	// ウィンドウの×ボタンが押されるまでループ
	while (true) {

		// Windowsのメッセージ処理
		if (winApp->ProcessMessage()) {
			// ゲームループを抜ける
			break;
		}

	#pragma region 更新処理

		// 入力の更新
		input->Update();

		{
			// ===== キー入力による操作 =====
			const float moveSpeed = 0.05f;
			const float rotateSpeed = 0.02f;

			// 現在のTransform取得
			// math::Vector3 planePos = planeObject->GetTranslate();
			// math::Vector3 planeRot = planeObject->GetRotate();

			math::Vector3 fencePos = fenceObject->GetTranslate();
			math::Vector3 fenceRot = fenceObject->GetRotate();

			// 前後移動
			if (input->PushKey(DIK_W))
			{
				fencePos.y += moveSpeed;
			}

			if (input->PushKey(DIK_S))
			{
				fencePos.y -= moveSpeed;
			}

			// 左右移動
			if (input->PushKey(DIK_A))
			{
				fencePos.x -= moveSpeed;
			}

			if (input->PushKey(DIK_D))
			{
				fencePos.x += moveSpeed;
			}

			// 回転（Z軸）
			if (input->PushKey(DIK_Q))
			{
				fenceRot.z -= rotateSpeed;
			}

			if (input->PushKey(DIK_E))
			{
				fenceRot.z += rotateSpeed;
			}

			// Transform反映
			fenceObject->SetTranslate(fencePos);
			fenceObject->SetRotate(fenceRot);
		}

		// 3Dオブジェクト更新
		fenceObject->Update();
		fenceObject->Update();

		// スプライト更新
		uvCheckerSprite->Update();

		skybox->Update();

	#pragma endregion

	#ifdef USE_IMGUI
#pragma region ImGui操作UI

		// フレーム開始
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Control Panel");

		if (ImGui::CollapsingHeader("Skybox"))
		{
			// ========================================
			// カメラ回転
			// ========================================
			math::Vector3 rotate = skybox->GetCameraRotate();

			if (ImGui::DragFloat3(
				"Camera Rotate##Skybox",
				&rotate.x,
				0.01f))
			{
				skybox->SetCameraRotate(rotate);
			}

			// ========================================
			// カメラ位置
			// ========================================
			math::Vector3 translate = skybox->GetCameraTranslate();

			if (ImGui::DragFloat3(
				"Camera Position##Skybox",
				&translate.x,
				0.1f))
			{
				skybox->SetCameraTranslate(translate);
			}

			// ========================================
			// 初期状態に戻す
			// ========================================
			if (ImGui::Button("Reset Skybox Camera"))
			{
				skybox->SetCameraRotate(
					{ 0.3f, 0.0f, 0.0f }
				);

				skybox->SetCameraTranslate(
					{ 0.0f, 4.0f, -10.0f }
				);
			}
		}

		ImGui::End();

		// ImGui描画コマンド生成
		ImGui::Render();

#pragma endregion
	#endif

	#pragma region 描画処理

		// DirectX描画準備
		dxCommon->PreDraw();

		skybox->Draw();

		// 3Dオブジェクト描画準備
		object3dCommon->SetCommonRenderSetting();

		// 3Dオブジェクト描画
		// planeObject->Draw();
		// bunnyObject->Draw();
		fenceObject->Draw();

		// スプライト描画準備
		spriteCommon->SetupCommonDrawing();

		// スプライト描画
		//uvCheckerSprite->Draw();

	#ifdef USE_IMGUI
		// ImGui描画コマンドを積む
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon->GetCommandList());
	#endif

		// 描画後処理
		dxCommon->PostDraw();

		// 中間リソース解放
		TextureManager::GetInstance()->ReleaseIntermediateResources();

	#pragma endregion
	}

#pragma endregion

#pragma region 解放処理

	// 3Dオブジェクト解放
	delete fenceObject;
	fenceObject = nullptr;

	delete skybox;
	delete skyboxCommon;

	/*delete bunnyObject;
	bunnyObject = nullptr;

	delete planeObject;
	planeObject = nullptr;*/

	// 3Dモデルマネージャー終了
	ModelManager::GetInstance()->Finalize();

	delete modelCommon;
	modelCommon = nullptr;

	delete object3dCommon;
	object3dCommon = nullptr;

	// スプライト解放
	delete uvCheckerSprite;

	delete spriteCommon;
	spriteCommon = nullptr;

	// テクスチャ管理終了
	TextureManager::GetInstance()->Finalize();

#ifdef USE_IMGUI
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif

	// 入力解放
	delete input;
	input = nullptr;

	// オーディオ解放
	SoundUnload(&soundData1);
	xAudio2.Reset();  // ComPtrのリセットで解放

	// DirectX解放
	delete dxCommon;
	dxCommon = nullptr;

	// ウィンドウ解放
	winApp->Finalize();
	delete winApp;
	winApp = nullptr;

#pragma endregion

	return 0;
}