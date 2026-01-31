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

	// WinAppポインタ作成
	WinApp *winApp = nullptr;

	// Windows API 初期化
	winApp = new WinApp();
	winApp->Initialize();


	// ===== DirectX関連 =====

	// DirectXCommonポインタ作成
	DirectXCommon *dxCommon = nullptr;

	// DirectX初期化（ウィンドウ情報を渡す）
	dxCommon = new DirectXCommon();
	dxCommon->Initialize(winApp);


	// ===== 3Dオブジェクト共通部初期化 =====

	// Object3dCommonポインタ作成
	Object3dCommon *object3dCommon = nullptr;
	object3dCommon = new Object3dCommon();
	object3dCommon->Initialize(dxCommon);


	// ===== モデル共通部初期化 =====

	// ModelCommonポインタ作成
	ModelCommon *modelCommon = nullptr;
	modelCommon = new ModelCommon();
	modelCommon->Initialize(dxCommon);


	// ===== テクスチャ管理 =====

	// DirectXCommonをTextureManagerにセット
	TextureManager::GetInstance()->SetDirectXCommon(dxCommon);

	// テクスチャマネージャ初期化
	TextureManager::GetInstance()->Initialize();


	// ===== スプライト共通処理 =====

	// SpriteCommonポインタ作成
	SpriteCommon *spriteCommon = nullptr;

	// スプライト共通部初期化（描画に必要）
	spriteCommon = new SpriteCommon();
	spriteCommon->Initialize(dxCommon);


	// ===== オーディオ（XAudio2） =====

	// XAudio2エンジン生成
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
	IXAudio2MasteringVoice *masterVoice = nullptr;

	HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);

	// マスターボイス生成（音声出力のルート）
	result = xAudio2->CreateMasteringVoice(&masterVoice);

	// 音声データ読み込み
	SoundData soundData1 = SoundLoadWave("resources/audio/Alarm02.wav");

	// 音声再生
	SoundPlayWave(xAudio2.Get(), soundData1);


	// ===== 入力処理 =====

	// Inputポインタ作成
	Input *input = nullptr;

	// 入力初期化（ウィンドウ情報を渡す）
	input = new Input();
	input->Initialize(winApp);

#pragma endregion

#pragma region 最初のシーンの初期化

	// ===== 使用するテクスチャ一覧 =====
	std::vector<std::string> textures = {
		"resources/textures/uvChecker.png",
		"resources/textures/monsterball.png"
	};

	// ===== スプライト管理用配列 =====
	std::vector<Sprite *> sprites;

	// ===== スプライトを6枚生成 =====
	for (uint32_t i = 0; i < 6; ++i) {
		Sprite *sprite = new Sprite();

		// 2種類のテクスチャを交互に使用（0,1,0,1...）
		std::string &textureFile = textures[i % 2];

		// スプライト初期化
		sprite->Initialize(spriteCommon, textureFile);

		// 反転設定（左右・上下ともなし）
		sprite->SetIsFlipX(false);
		sprite->SetIsFlipY(false);

		// 使用するテクスチャ領域（左上座標とサイズ）
		sprite->SetTextureLeftTop({ 0.0f, 0.0f });
		sprite->SetTextureSize({ 64.0f, 64.0f });

		// スプライト表示サイズ
		sprite->SetSize({ 64.0f, 64.0f });

		// 表示位置（横方向に等間隔に配置）
		sprite->SetPosition({ 100.0f + i * 200.0f, 100.0f });

		// 管理配列に追加
		sprites.push_back(sprite);
	}

	// ===== 表示・ライティング切り替え用フラグ =====

	// ライティング制御フラグ
	bool useLighting = false;
	bool useLightingTriangle = false;
	bool useLightingSphere = false;

	// 表示対象切り替えフラグ
	bool showModelData = true;
	bool showTriangle = false;
	bool showSphere = false;
	bool showSprite = false;

	// 3Dモデルは1つだけ作る
	Model *model = new Model();
	model->Initialize(modelCommon);

	float offsetX = -2.0f;

	// Object3dを複数作る
	std::vector<Object3d *> objects;
	for (int i = 0; i < 2; ++i) {
		Object3d *obj = new Object3d();
		obj->Initialize(object3dCommon);
		obj->SetModel(model);

		obj->SetTranslate({ offsetX + float(i * 4), 0.0f, 0.0f });
		obj->SetRotate({ 0.0f, 0.0f, 0.0f });
		obj->SetScale({ 1.0f, 1.0f, 1.0f });

		objects.push_back(obj);
	}

#pragma endregion

	D3DResourceLeakChecker leakCheck;

	//#pragma region Resources: 3D Object (ModelData)
	//
	//	/**************************************************
	//	* 頂点データの作成とビュー
	//	**************************************************/
	//
	//	// モデル読み込み
	//	ModelData modelData = LoadObjFile("resources/models/plane", "plane.obj");
	//	//----VertexResourceを生成する----
	//	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = dxCommon->CreateBufferResource(sizeof(VertexData) * modelData.vertices.size());
	//
	//	//----VertexBufferViewを生成する----
	//
	//	// 頂点バッファビューを作成する
	//	D3D12_VERTEX_BUFFER_VIEW vertexBufferView {};
	//	// リソースの先頭のアドレスから使う
	//	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	//	// 使用するリソースのサイズは頂点6つ分のサイズ
	//	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
	//	// 1頂点あたりのサイズ
	//	vertexBufferView.StrideInBytes = sizeof(VertexData);
	//
	//	//----Resourceにデータを書き込む----
	//
	//	VertexData *vertexData = nullptr;
	//	vertexResource->Map(0, nullptr, reinterpret_cast<void **>(&vertexData)); // 書き込むためのアドレスを取得
	//	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size()); // 頂点データをリソースにコピー
	//
	//	/**************************************************
	//	* Transform
	//	**************************************************/
	//
	//	// Transform変数を作る
	//	Transform transform { {1.0f,1.0f,1.0f},{0.0f,3.14f,0.0f},{0.0f,0.0f,0.0f} };
	//
	//	// camera変数を作る
	//	Transform cameraTransform { {1.0f, 1.0f, 1.0f},{0.0f, 0.0f, 0.0f},{0.0f,0.0f,-10.0f} };
	//
	//	/**************************************************
	//	* TransformationMatrix用のResourceを作る
	//	**************************************************/
	//
	//	// WVP用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	//	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource = dxCommon->CreateBufferResource(sizeof(ObjectMatrices));
	//	// データを書き込む
	//	ObjectMatrices *wvpData = nullptr;
	//	// 書き込むためのアドレスを取得
	//	wvpResource->Map(0, nullptr, reinterpret_cast<void **>(&wvpData));
	//	// 単位行列を書き込んでおく
	//	wvpData->WVP = MakeIdentity4x4();
	//	wvpData->World = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	//
	//	/**************************************************
	//	* Material用のResourceを作る
	//	**************************************************/
	//
	//	// マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	//	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = dxCommon->CreateBufferResource(sizeof(Material));
	//	// マテリアルにデータを書き込む
	//	Material *materialData = nullptr;
	//	// 書き込むためのアドレスを取得
	//	materialResource->Map(0, nullptr, reinterpret_cast<void **>(&materialData));
	//	// 今回は赤を書き込んでみる
	//	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	//	materialData->enableLighting = false;
	//	materialData->uvTransform = MakeIdentity4x4();
	//
	//	/**************************************************
	//	* ランバート反射
	//	**************************************************/
	//
	//	// マテリアルリソースを作る
	//	Microsoft::WRL::ComPtr<ID3D12Resource> resourceDirectionalLight = dxCommon->CreateBufferResource(sizeof(DirectionalLight));
	//
	//	// directionalLight用のリソースを作る
	//	DirectionalLight *directionalLightData = nullptr;
	//
	//	resourceDirectionalLight->Map(0, nullptr, reinterpret_cast<void **>(&directionalLightData));
	//
	//	// デフォルト値
	//	directionalLightData->color = { 1.0f,1.0f,1.0f,1.0f };
	//	directionalLightData->direction = { 0.0f,-1.0f,0.0f };
	//	directionalLightData->intensity = 1.0f;
	//
	//#pragma endregion
	//
	//#pragma region Resources: 3D Object (Triangle)
	//
	//	/**************************************************
	//	* 頂点データの作成とビュー
	//	**************************************************/
	//
	//	//----VertexResourceを生成する----
	//
	//	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceTriangle = dxCommon->CreateBufferResource(sizeof(VertexData) * 3);
	//
	//	//----VertexBufferViewを生成する----
	//
	//	// 頂点バッファビューを作成する
	//	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewTriangle {};
	//	// リソースの先頭のアドレスから使う
	//	vertexBufferViewTriangle.BufferLocation = vertexResourceTriangle->GetGPUVirtualAddress();
	//	// 使用するリソースのサイズは頂点6つ分のサイズ
	//	vertexBufferViewTriangle.SizeInBytes = sizeof(VertexData) * 3;
	//	// 1頂点あたりのサイズ
	//	vertexBufferViewTriangle.StrideInBytes = sizeof(VertexData);
	//
	//	//----Resourceにデータを書き込む----
	//
	//	// 頂点リソースにデータを書き込む
	//	VertexData *vertexDataTriangle = nullptr;
	//	// 書き込むためのアドレスを取得
	//	vertexResourceTriangle->Map(0, nullptr, reinterpret_cast<void **>(&vertexDataTriangle));
	//	// 左下
	//	vertexDataTriangle[0].position = { -0.5f,-0.5f,0.0f,1.0f };
	//	vertexDataTriangle[0].texcoord = { 0.0f,1.0f };
	//	vertexDataTriangle[0].normal = { 0.0f, 0.0f,-1.0f };
	//	// 上
	//	vertexDataTriangle[1].position = { 0.0f,0.5f,0.0f,1.0f };
	//	vertexDataTriangle[1].texcoord = { 0.5f,0.0f };
	//	vertexDataTriangle[1].normal = { 0.0f, 0.0f,-1.0f };
	//	// 右下
	//	vertexDataTriangle[2].position = { 0.5f,-0.5f,0.0f,1.0f };
	//	vertexDataTriangle[2].texcoord = { 1.0f,1.0f };
	//	vertexDataTriangle[2].normal = { 0.0f, 0.0f,-1.0f };
	//
	//	/**************************************************
	//	* IndexResource
	//	**************************************************/
	//
	//	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceTriangle = dxCommon->CreateBufferResource(sizeof(uint32_t) * 3);
	//
	//	D3D12_INDEX_BUFFER_VIEW indexBufferViewTriangle {};
	//	indexBufferViewTriangle.BufferLocation = indexResourceTriangle->GetGPUVirtualAddress();
	//	indexBufferViewTriangle.SizeInBytes = sizeof(uint32_t) * 3;
	//	indexBufferViewTriangle.Format = DXGI_FORMAT_R32_UINT;
	//
	//	uint32_t *indexDataTriangle = nullptr;
	//	indexResourceTriangle->Map(0, nullptr, reinterpret_cast<void **>(&indexDataTriangle));
	//	indexDataTriangle[0] = 0;
	//	indexDataTriangle[1] = 1;
	//	indexDataTriangle[2] = 2;
	//
	//	/**************************************************
	//	* Transform
	//	**************************************************/
	//
	//	// Transform変数を作る
	//	Transform transformTriangle { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	//
	//	// camera変数を作る
	//	Transform cameraTransformTriangle { {1.0f, 1.0f, 1.0f},{0.0f, 0.0f, 0.0f},{0.0f,0.0f,-5.0f} };
	//
	//	/**************************************************
	//	* TransformationMatrix用のResourceを作る
	//	**************************************************/
	//
	//	// WVP用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	//	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResourceTriangle = dxCommon->CreateBufferResource(sizeof(ObjectMatrices));
	//	// データを書き込む
	//	ObjectMatrices *wvpDataTriangle = nullptr;
	//	// 書き込むためのアドレスを取得
	//	wvpResourceTriangle->Map(0, nullptr, reinterpret_cast<void **>(&wvpDataTriangle));
	//	// 単位行列を書き込んでおく
	//	wvpDataTriangle->WVP = MakeIdentity4x4();
	//	wvpDataTriangle->World = MakeAffineMatrix(transformTriangle.scale, transformTriangle.rotate, transformTriangle.translate);
	//
	//	/**************************************************
	//	* Material用のResourceを作る
	//	**************************************************/
	//
	//	// マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	//	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceTriangle = dxCommon->CreateBufferResource(sizeof(Material));
	//	// マテリアルにデータを書き込む
	//	Material *materialDataTriangle = nullptr;
	//	// 書き込むためのアドレスを取得
	//	materialResourceTriangle->Map(0, nullptr, reinterpret_cast<void **>(&materialDataTriangle));
	//	// 今回は赤を書き込んでみる
	//	materialDataTriangle->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	//	materialDataTriangle->enableLighting = false;
	//	materialDataTriangle->uvTransform = MakeIdentity4x4();
	//
	//	/**************************************************
	//	* ランバート反射
	//	**************************************************/
	//
	//	// マテリアルリソースを作る
	//	Microsoft::WRL::ComPtr<ID3D12Resource> resourceDirectionalLightTriangle = dxCommon->CreateBufferResource(sizeof(DirectionalLight));
	//
	//	// directionalLight用のリソースを作る
	//	DirectionalLight *directionalLightDataTriangle = nullptr;
	//
	//	resourceDirectionalLightTriangle->Map(0, nullptr, reinterpret_cast<void **>(&directionalLightDataTriangle));
	//
	//	// デフォルト値
	//	directionalLightDataTriangle->color = { 1.0f,1.0f,1.0f,1.0f };
	//	directionalLightDataTriangle->direction = { 0.0f,-1.0f,0.0f };
	//	directionalLightDataTriangle->intensity = 1.0f;
	//
	//#pragma endregion
	//
	//#pragma region Resources: 3D Object (Sphere)
	//
	//	// 球の分割数と頂点数計算
	//	uint32_t kSubdivision = 16;
	//	uint32_t vertexCount = (kSubdivision + 1) * (kSubdivision + 1);
	//	uint32_t indexCount = kSubdivision * kSubdivision * 6;
	//
	//	/**************************************************
	//	* 頂点データの作成とビュー
	//	**************************************************/
	//
	//	// 頂点リソースの作成
	//	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSphere = dxCommon->CreateBufferResource(sizeof(VertexData) * vertexCount);
	//
	//	// 頂点バッファビューの作成
	//	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSphere {};
	//	vertexBufferViewSphere.BufferLocation = vertexResourceSphere->GetGPUVirtualAddress();
	//	vertexBufferViewSphere.SizeInBytes = sizeof(VertexData) * vertexCount;
	//	vertexBufferViewSphere.StrideInBytes = sizeof(VertexData);
	//
	//	// 頂点データのマップと生成
	//	VertexData *vertexDataSphere = nullptr;
	//	vertexResourceSphere->Map(0, nullptr, reinterpret_cast<void **>(&vertexDataSphere));
	//
	//	const float kLonEvery = std::numbers::pi_v<float> *2.0f / float(kSubdivision);
	//	const float kLatEvery = std::numbers::pi_v<float> / float(kSubdivision);
	//
	//	for (uint32_t lat = 0; lat <= kSubdivision; ++lat) {
	//		float latAngle = -std::numbers::pi_v<float> / 2.0f + lat * kLatEvery;
	//		for (uint32_t lon = 0; lon <= kSubdivision; ++lon) {
	//			float lonAngle = lon * kLonEvery;
	//
	//			uint32_t index = lat * (kSubdivision + 1) + lon;
	//
	//			Vector3 pos = {
	//				std::cosf(latAngle) * std::cosf(lonAngle),
	//				std::sinf(latAngle),
	//				std::cosf(latAngle) * std::sinf(lonAngle)
	//			};
	//
	//			vertexDataSphere[index] = {
	//				{ pos.x, pos.y, pos.z, 1.0f },
	//				{ float(lon) / kSubdivision, 1.0f - float(lat) / kSubdivision },
	//				{ pos.x, pos.y, pos.z }
	//			};
	//		}
	//	}
	//
	//	/**************************************************
	//	* IndexResource
	//	**************************************************/
	//
	//	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSphere = dxCommon->CreateBufferResource(sizeof(uint32_t) * indexCount);
	//
	//	D3D12_INDEX_BUFFER_VIEW indexBufferViewSphere {};
	//	indexBufferViewSphere.BufferLocation = indexResourceSphere->GetGPUVirtualAddress();
	//	indexBufferViewSphere.SizeInBytes = sizeof(uint32_t) * indexCount;
	//	indexBufferViewSphere.Format = DXGI_FORMAT_R32_UINT;
	//
	//	uint32_t *indexDataSphere = nullptr;
	//	indexResourceSphere->Map(0, nullptr, reinterpret_cast<void **>(&indexDataSphere));
	//
	//	uint32_t index = 0;
	//	for (uint32_t lat = 0; lat < kSubdivision; ++lat) {
	//		for (uint32_t lon = 0; lon < kSubdivision; ++lon) {
	//
	//			uint32_t a = lat * (kSubdivision + 1) + lon;
	//			uint32_t b = (lat + 1) * (kSubdivision + 1) + lon;
	//			uint32_t c = lat * (kSubdivision + 1) + (lon + 1);
	//			uint32_t d = (lat + 1) * (kSubdivision + 1) + (lon + 1);
	//
	//			// 三角形1: A-B-C
	//			indexDataSphere[index++] = a;
	//			indexDataSphere[index++] = b;
	//			indexDataSphere[index++] = c;
	//
	//			// 三角形2: C-B-D
	//			indexDataSphere[index++] = c;
	//			indexDataSphere[index++] = b;
	//			indexDataSphere[index++] = d;
	//		}
	//	}
	//
	//	/**************************************************
	//	* Transform
	//	**************************************************/
	//
	//	Transform transformSphereInit;
	//
	//	// SphereTransform変数を作る
	//	Transform transformSphere { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	//	transformSphereInit = transformSphere;
	//
	//	// SphereCamera変数を作る
	//	Transform cameraTransformSphere { {1.0f, 1.0f, 1.0f},{0.0f, 0.0f, 0.0f},{0.0f,0.0f,-10.0f} };
	//
	//	/**************************************************
	//	* TransformationMatrix用のResourceを作る
	//	**************************************************/
	//
	//	// 1つ分のサイズを用意する
	//	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResourceSphere = dxCommon->CreateBufferResource(sizeof(ObjectMatrices));
	//	// データを書き込む
	//	ObjectMatrices *wvpDataSphere = nullptr;
	//	// 書き込むためのアドレスを取得
	//	wvpResourceSphere->Map(0, nullptr, reinterpret_cast<void **>(&wvpDataSphere));
	//	// 単位行列を書き込んでおく
	//	wvpDataSphere->WVP = MakeIdentity4x4();
	//	wvpDataSphere->World = MakeAffineMatrix(transformSphere.scale, transformSphere.rotate, transformSphere.translate);
	//
	//	/**************************************************
	//	* Material用のResourceを作る
	//	**************************************************/
	//
	//	// マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	//	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSphere = dxCommon->CreateBufferResource(sizeof(Material));
	//	Material *materialDataSphere = nullptr;
	//	materialResourceSphere->Map(0, nullptr, reinterpret_cast<void **>(&materialDataSphere));
	//	materialDataSphere->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	//	materialDataSphere->enableLighting = false;
	//	materialDataSphere->uvTransform = MakeIdentity4x4();
	//
	//	/**************************************************
	//	* ランバート反射
	//	**************************************************/
	//
	//	// マテリアルリソースを作る
	//	Microsoft::WRL::ComPtr<ID3D12Resource> resourceDirectionalLightSphere = dxCommon->CreateBufferResource(sizeof(DirectionalLight));
	//
	//	// directionalLight用のリソースを作る
	//	DirectionalLight *directionalLightDataSphere = nullptr;
	//
	//	resourceDirectionalLightSphere->Map(0, nullptr, reinterpret_cast<void **>(&directionalLightDataSphere));
	//
	//	// デフォルト値
	//	directionalLightDataSphere->color = { 1.0f,1.0f,1.0f,1.0f };
	//	directionalLightDataSphere->direction = { 0.0f,-1.0f,0.0f };
	//	directionalLightDataSphere->intensity = 1.0f;
	//
	//#pragma endregion

	//#pragma region テクスチャを読み込んで使う
	//
	//	// Textureを読んで転送する
	//	DirectX::ScratchImage mipImages = dxCommon->LoadTexture("resources/textures/uvChecker.png");
	//	const DirectX::TexMetadata &metadata = mipImages.GetMetadata();
	//	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource = dxCommon->CreateTextureResource(metadata);
	//	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = dxCommon->UploadTextureData(textureResource, mipImages);
	//
	//	// 2枚目のTextureを読んで転送する
	//	DirectX::ScratchImage mipImages2 = dxCommon->LoadTexture(modelData.material.textureFilePath);
	//	const DirectX::TexMetadata &metadata2 = mipImages2.GetMetadata();
	//	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2 = dxCommon->CreateTextureResource(metadata2);
	//	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource2 = dxCommon->UploadTextureData(textureResource2, mipImages2);
	//
	//#pragma endregion

	//#pragma region ShaderResourceViewを作る
	//
	//	// metaDataを基にSRVの設定
	//	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
	//	srvDesc.Format = metadata.format;
	//	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	//	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	//	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);
	//
	//	// SRVを作成するDescriptorHeapの場所を決める
	//	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = dxCommon->GetSRVCPUDescriptorHandle(1);
	//	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = dxCommon->GetSRVGPUDescriptorHandle(1);
	//	// SRVの生成
	//	dxCommon->GetDevice()->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);
	//
	//	// 2枚目のmetaDataを基にSRVの設定
	//	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2 {};
	//	srvDesc2.Format = metadata2.format;
	//	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	//	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	//	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);
	//
	//	// 2枚目のSRVを作成するDescriptorHeapの場所を決める
	//	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = dxCommon->GetSRVCPUDescriptorHandle(2);
	//	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = dxCommon->GetSRVGPUDescriptorHandle(2);
	//	// 2枚目のSRVの生成
	//	dxCommon->GetDevice()->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);
	//
	//#pragma endregion

#pragma region メインループ

	// ウィンドウの×ボタンが押されるまでループ
	while (true) {

		// Windowsのメッセージ処理
		if (winApp->ProcessMessage()) {
			// ゲームループを抜ける
			break;
		}

		// ゲームの処理

		// 入力の更新
		input->Update();

		/*if (input->PushKey(DIK_W))
		{
			transform.translate.y += 0.01f;
		}

		if (input->PushKey(DIK_S))
		{
			transform.translate.y -= 0.01f;
		}

		if (input->PushKey(DIK_A))
		{
			transform.translate.x -= 0.01f;
		}

		if (input->PushKey(DIK_D))
		{
			transform.translate.x += 0.01f;
		}

		if (input->PushKey(DIK_Q))
		{
			transform.rotate.y -= 0.01f;
		}

		if (input->PushKey(DIK_E))
		{
			transform.rotate.y += 0.01f;
		}*/

	#pragma region 更新: 3D Object (ModelData)

		for (size_t i = 0; i < objects.size(); ++i) {
			objects[i]->Update();
		}

	#pragma endregion

	#pragma region 更新: 2D Object (Sprite)

		for (uint32_t i = 0; i < sprites.size(); ++i) {
			sprites[i]->Update();
		}

	#pragma endregion

	#ifdef USE_IMGUI
		// フレームの開始
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	#endif

	#ifdef USE_IMGUI
		ImGui::Begin("Settings");

		// スプライト選択＆操作
		static int currentSprite = 0;
		ImGui::SliderInt("Sprite Index", &currentSprite, 0, static_cast<int>(sprites.size()) - 1);
		Sprite *sprite = sprites[currentSprite];

		Vector2 size = sprite->GetSize();
		float rotation = sprite->GetRotation();
		Vector2 position = sprite->GetPosition();
		Vector4 color = sprite->GetColor();

		ImGui::DragFloat2("Sprite Size", &size.x, 1.0f, 0.0f, 1000.0f);
		ImGui::SliderAngle("Sprite Rotation", &rotation);
		ImGui::DragFloat2("Sprite Position", &position.x, 1.0f);
		ImGui::ColorEdit3("Sprite Color", &color.x);

		sprite->SetSize(size);
		sprite->SetRotation(rotation);
		sprite->SetPosition(position);
		sprite->SetColor(color);

		// 3Dオブジェクト選択＆操作
		static int currentObject = 0;
		ImGui::SliderInt("3D Object Index", &currentObject, 0, static_cast<int>(objects.size()) - 1);
		Object3d *obj = objects[currentObject];

		Vector3 scale = obj->GetScale();
		Vector3 rotate = obj->GetRotate();
		Vector3 translate = obj->GetTranslate();

		ImGui::DragFloat3("3D Scale", &scale.x, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat3("3D Rotation", &rotate.x, 0.5f, -360.0f, 360.0f);
		ImGui::DragFloat3("3D Translation", &translate.x, 0.1f);

		obj->SetScale(scale);
		obj->SetRotate(rotate);
		obj->SetTranslate(translate);

		ImGui::End();
	#endif

		//#pragma region ワールド・ビュー・プロジェクション行列計算: 3D Object (Triangle)

		//	//transform.rotate.y += 0.01f;
		//	Matrix4x4 worldMatrixTriangle = MakeAffineMatrix(transformTriangle.scale, transformTriangle.rotate, transformTriangle.translate);
		//	Matrix4x4 cameraMatrixTriangle = MakeAffineMatrix(cameraTransformTriangle.scale, cameraTransformTriangle.rotate, cameraTransformTriangle.translate);
		//	Matrix4x4 viewMatrixTriangle = Inverse(cameraMatrixTriangle);
		//	Matrix4x4 projectionMatrixTriangle = MakePerspectiveFovMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClientHeight), 0.1f, 100.0f);
		//	Matrix4x4 worldViewProjectionMatrixTriangle = Multiply(worldMatrixTriangle, Multiply(viewMatrixTriangle, projectionMatrixTriangle));
		//	wvpDataTriangle->WVP = worldViewProjectionMatrixTriangle;
		//	wvpDataTriangle->World = worldMatrixTriangle;

		//#pragma endregion

		//#pragma region ワールド・ビュー・プロジェクション行列計算: 3D Object (Sphere)

		//	transformSphere.rotate.y += 0.01f;
		//	Matrix4x4 worldMatrixSphere = MakeAffineMatrix(transformSphere.scale, transformSphere.rotate, transformSphere.translate);
		//	Matrix4x4 cameraMatrixSphere = MakeAffineMatrix(cameraTransformSphere.scale, cameraTransformSphere.rotate, cameraTransformSphere.translate);
		//	Matrix4x4 viewMatrixSphere = Inverse(cameraMatrixSphere);
		//	Matrix4x4 projectionMatrixSphere = MakePerspectiveFovMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClientHeight), 0.1f, 100.0f);
		//	Matrix4x4 worldViewProjectionMatrixSphere = Multiply(worldMatrixSphere, Multiply(viewMatrixSphere, projectionMatrixSphere));
		//	wvpDataSphere->WVP = worldViewProjectionMatrixSphere;
		//	wvpDataSphere->World = worldMatrixSphere;

		//#pragma endregion

	#ifdef USE_IMGUI
		// ImGuiの内部コマンドを生成する
		ImGui::Render();
	#endif

	#pragma region 画面をクリアする

		// DirectXの描画準備。全ての描画に共通のグラフィックスコマンドを積む
		dxCommon->PreDraw();

		// Spriteの描画準備。Spriteの描画に共通のグラフィックスコマンドを積む
		spriteCommon->SetupCommonDrawing();

		// 3Dオブジェクトの描画準備。3Dオブジェクトの描画に共通のグラフィックスコマンドを積む
		object3dCommon->SetCommonRenderSetting();

	#pragma region 描画: 3D Object (ModelData)

		for (size_t i = 0; i < objects.size(); ++i) {
			objects[i]->Draw();
		}

	#pragma endregion

		//#pragma region 描画: 3D Object (Triangle)

		//	if (showTriangle) {

		//		dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferViewTriangle); // VBVを設定
		//		dxCommon->GetCommandList()->IASetIndexBuffer(&indexBufferViewTriangle); // IBVを設定

		//		// 形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えておけばいい
		//		dxCommon->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		//		// マテリアルCBufferの場所を設定
		//		dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResourceTriangle->GetGPUVirtualAddress());
		//		// wvp用のCBufferの場所を設定
		//		dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, wvpResourceTriangle->GetGPUVirtualAddress());
		//		// SRVのDescriptorTableの先頭を設定。2はrootParameter[2]である。
		//		dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
		//		dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, resourceDirectionalLightTriangle->GetGPUVirtualAddress());

		//		dxCommon->GetCommandList()->DrawIndexedInstanced(3, 1, 0, 0, 0);

		//	}

		//#pragma endregion

		//#pragma region 描画: 3D Object (Sphere)

		//	if (showSphere) {

		//		dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferViewSphere); // VBVを設定
		//		dxCommon->GetCommandList()->IASetIndexBuffer(&indexBufferViewSphere); // IBVを設定
		//		dxCommon->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		//		dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResourceSphere->GetGPUVirtualAddress());
		//		dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, wvpResourceSphere->GetGPUVirtualAddress());
		//		dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU2);
		//		dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, resourceDirectionalLightSphere->GetGPUVirtualAddress());

		//		dxCommon->GetCommandList()->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
		//	}

		//#pragma endregion

	#pragma region 描画: 2D Object (Sprite)

		for (uint32_t i = 0; i < sprites.size(); ++i) {
			sprites[i]->Draw();
		}

	#pragma endregion

	#ifdef USE_IMGUI
		// 実際のcommandListのImGuiの描画コマンドを積む
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon->GetCommandList());
	#endif

		// 描画後処理
		dxCommon->PostDraw();

		TextureManager::GetInstance()->ReleaseIntermediateResources();

	#pragma endregion
	}

#pragma endregion

#pragma region Object解放

	// モデル関連の解放
	delete model;
	model = nullptr;

	delete modelCommon;
	modelCommon = nullptr;

	// ===== 3Dオブジェクト =====
	for (Object3d *object : objects) {
		delete object;
	}
	objects.clear();

	delete object3dCommon;
	object3dCommon = nullptr;

	// ===== スプライト =====
	// スプライト配列の中身を順に解放してからクリア
	for (Sprite *sprite : sprites) {
		delete sprite;
	}
	sprites.clear();

	delete spriteCommon;
	spriteCommon = nullptr;

	// ===== テクスチャ管理 =====
	TextureManager::GetInstance()->Finalize();

	// ===== ImGui =====
#ifdef USE_IMGUI
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif

	// ===== 入力 =====
	delete input;
	input = nullptr;

	// ===== オーディオ =====
	SoundUnload(&soundData1);
	xAudio2.Reset();  // ComPtrのリセットで解放

	// ===== DirectX =====
	delete dxCommon;
	dxCommon = nullptr;

	// ===== WinApp =====
	winApp->Finalize();
	delete winApp;
	winApp = nullptr;

#pragma endregion

	return 0;
}