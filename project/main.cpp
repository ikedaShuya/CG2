#include <string>
#include <format>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <dbghelp.h>
#include <strsafe.h>
#include <dxgidebug.h>
#include <dxcapi.h>
#include <vector>
#include <numbers>
#include <wrl.h>
#include <xaudio2.h>

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"Dbghelp.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"dxcompiler.lib")
#pragma comment(lib,"xaudio2.lib")

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

#include "externals/DirectXTex/DirectXTex.h"

#include "externals/DirectXTex/d3dx12.h"

#include "Math.h"

#include "Input.h"

#include "WinApp.h"

using namespace Math;

struct D3DResourceLeakChecker
{
	~D3DResourceLeakChecker()
	{
		// リソースリークチェック
		Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))))
		{
			debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		}
	}
};

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
	BYTE* pBuffer;
	// バッファのサイズ
	unsigned int bufferSize;
};

SoundData SoundLoadWave(const char* filename)
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
	file.read((char*)&riff, sizeof(riff));
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
	file.read((char*)&format, sizeof(ChunkHeader));
	if (strncmp(format.chunk.id, "fmt ", 4) != 0) {
		assert(0);
	}

	// チャンク本体の読み込み
	assert(format.chunk.size <= sizeof(format.fmt));
	file.read((char*)&format.fmt, format.chunk.size);

	// Dataチャンクの読み込み
	ChunkHeader data;
	file.read((char*)&data, sizeof(data));
	// JUNKチャンクを検出した場合
	if (strncmp(data.id, "JUNK", 4) == 0) {
		// 読み取り位置をJUNKチャンクの終わりまで進める
		file.seekg(data.size, std::ios_base::cur);
		// 再読み込み
		file.read((char*)&data, sizeof(data));
	}

	if (strncmp(data.id, "data", 4) != 0) {
		assert(0);
	}

	// Dataチャンクのデータ部（波形データ）の読み込み
	char* pBuffer = new char[data.size];
	file.read(pBuffer, data.size);

	//----ファイルクローズ----

	// Waveファイルを閉じる
	file.close();

	//----読み込んだ音声データをreturn----

	// returnする為の音声データ
	SoundData soundData = {};

	soundData.wfex = format.fmt;
	soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
	soundData.bufferSize = data.size;

	return soundData;
}

// 音声データを解放
void SoundUnload(SoundData* soundData)
{
	// バッファのメモリを解放
	delete[] soundData->pBuffer;

	soundData->pBuffer = 0;
	soundData->bufferSize = 0;
	soundData->wfex = {};
}

// 音楽再生
void SoundPlayWave(IXAudio2* xAudio2, const SoundData& soundData) {

	HRESULT result;

	// 波形フォーマットを元にSourceVoiceの生成
	IXAudio2SourceVoice* pSourceVoice = nullptr;
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

#pragma region math

struct Transform
{
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

struct VertexData
{
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

struct Material
{
	Vector4 color;
	int32_t enableLighting;
	float padding[3];
	Matrix4x4 uvTransform;
};

struct TransformationMatrix
{
	Matrix4x4 WVP;
	Matrix4x4 World;
};

struct DirectionalLight
{
	Vector4 color; //!< ライトの色
	Vector3 direction; //!< ライトの向き
	float intensity; //!< 輝度
};

struct MaterialData
{
	std::string textureFilePath;
};

struct ModelData
{
	std::vector<VertexData> vertices;
	MaterialData material;
};

#pragma endregion

#pragma region ログ

void Log(std::ostream& os, const std::string& message) {
	os << message << std::endl;
	OutputDebugStringA(message.c_str());
}

std::wstring ConvertString(const std::string& str) {
	if (str.empty()) {
		return std::wstring();
	}

	auto sizeNeeded =
		MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]),
			static_cast<int>(str.size()), NULL, 0);
	if (sizeNeeded == 0) {
		return std::wstring();
	}
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]),
		static_cast<int>(str.size()), &result[0], sizeNeeded);
	return result;
}

std::string ConvertString(const std::wstring& str) {
	if (str.empty()) {
		return std::string();
	}

	auto sizeNeeded =
		WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()),
			NULL, 0, NULL, NULL);
	if (sizeNeeded == 0) {
		return std::string();
	}
	std::string result(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()),
		result.data(), sizeNeeded, NULL, NULL);
	return result;
}

#pragma endregion

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
static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {

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

#pragma region ShaderResources

// CompileShader
Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
	// CompilerするShaderファイルへのパス
	const std::wstring& filePath,
	// Compilerに使用するProfile
	const wchar_t* profile,
	// 初期化で生成したものを3つ
	const Microsoft::WRL::ComPtr<IDxcUtils>& dxcUtils,
	const Microsoft::WRL::ComPtr<IDxcCompiler3>& dxcCompiler,
	const Microsoft::WRL::ComPtr<IDxcIncludeHandler>& includeHandler,
	std::ostream& os)
{
	//----hlslファイルを読み込む----

	// これからシェーダーをコンパイルする旨をログに出す
	Log(os, ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)));
	// hlslファイルを読む
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	// 読めなかったら止める
	assert(SUCCEEDED(hr));
	// 読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8; // UTF8の文字コードであることを通知

	//----Compileする----

	LPCWSTR arguments[] = {
		filePath.c_str(), // コンパイル対象のhlslファイル名
		L"-E",L"main", // エントリーポイントの指定。基本的にmain以外にはしない
		L"-T",profile, // ShaderProfileの設定
		L"-Zi",L"-Qembed_debug", // デバック用の情報を埋め込む
		L"-Od",    // 最適化を外しておく
		L"-Zpr",   // メモリレイアウトは行優先
	};
	// 実際にShaderをコンパイルする
	IDxcResult* shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer,    // 読み込んだファイル
		arguments,              // コンパイルオプション
		_countof(arguments),    // コンパイルオプションの数
		includeHandler.Get(),         // includeが含まれた諸々
		IID_PPV_ARGS(&shaderResult) // コンパイル結果
	);
	// コンパイルエラーではなくdxcが起動できないなど致命的な状況
	assert(SUCCEEDED(hr));

	//----警告・エラーがでていないか確認----

	// 警告・エラーが出てたらログに出して止める
	Microsoft::WRL::ComPtr<IDxcBlobUtf8> shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Log(os, shaderError->GetStringPointer());
		// 警告・エラーダメゼッタイ
		assert(false);
	}

	//----Compile結果を受け取って返す----

	// コンパイル結果から実行用のバイナリ部分を取得
	Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));
	// 成功したログを出す
	Log(os, ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile)));
	// もう使わないリソースを解放
	shaderSource->Release();
	shaderResult->Release();
	// 実行用のバイナリを返却
	return shaderBlob;
}

// DescriptorHeapの作成
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
	const Microsoft::WRL::ComPtr<ID3D12Device>& device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible)
{
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc {};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));
	return descriptorHeap;
}

D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}

#pragma endregion

#pragma region TextureResources

// Resource作成の関数
Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, size_t sizeInBytes) {

	// 頂点リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties {};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // UploadHeapを使う
	// 頂点リソースの設定
	D3D12_RESOURCE_DESC vertexResourceDesc {};
	// バッファリソース。テクスチャの場合はまた別の設定をする
	vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResourceDesc.Width = sizeInBytes; // リソースのサイズ。今回はVector4を3頂点分
	// バッファの場合はこれらは1にする決まり
	vertexResourceDesc.Height = 1;
	vertexResourceDesc.DepthOrArraySize = 1;
	vertexResourceDesc.MipLevels = 1;
	vertexResourceDesc.SampleDesc.Count = 1;
	// バッファの場合はこれにする決まり
	vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	// 実際に頂点リソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = nullptr;
	HRESULT hr = device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
		&vertexResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&vertexResource));
	assert(SUCCEEDED(hr));

	return vertexResource;
}

// Textureデータを読む
DirectX::ScratchImage LoadTexture(const std::string& filePath)
{
	// テクスチャファイルを読んでプログラムで扱えるようにする
	DirectX::ScratchImage image {};
	std::wstring filePathW = ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	// ミップマップの作成
	DirectX::ScratchImage mipImages {};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	// ミップマップ付きのデータを返す
	return mipImages;
}

// DirectX12のTextureResourceを作る
Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, const DirectX::TexMetadata& metadata)
{
	// metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc {};
	resourceDesc.Width = UINT(metadata.width); // Textureの幅
	resourceDesc.Height = UINT(metadata.height); // Textureの高さ
	resourceDesc.MipLevels = UINT16(metadata.mipLevels); // mipmapの数
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize); // 奥行き or 配列Textureの配列数
	resourceDesc.Format = metadata.format; // TextureのFormat
	resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定。
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension); // Textureの次元数。普段使っているのは2次元

	// 利用するHeapの設定。非常に特殊な運用。02_04exで一般的なケース版がある
	D3D12_HEAP_PROPERTIES heapProperties {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; //細かい設定を行う

	// Resourceの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties, // Heapの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし。
		&resourceDesc, // Resourceの設定
		D3D12_RESOURCE_STATE_COPY_DEST, // データ転送される設定
		nullptr, // Clear最適地。使わないのでnullptr
		IID_PPV_ARGS(&resource)); // 作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));
	return resource;
}

// TextureResourceにデータを転送する
[[nodiscard]]
Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages, const Microsoft::WRL::ComPtr<ID3D12Device>& device,
	const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList)
{
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(device.Get(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
	uint64_t intermediateSize = GetRequiredIntermediateSize(texture.Get(), 0, UINT(subresources.size()));
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource(device, intermediateSize);
	UpdateSubresources(commandList.Get(), texture.Get(), intermediateResource.Get(), 0, 0, UINT(subresources.size()), subresources.data());
	// Tetureへの転送後は利用できるよう、D3D12_RESOURCE_STATE_COPY_DESTからD3D12_RESOURCE_STATE_GENERIC_READへResourceStateを変更する
	D3D12_RESOURCE_BARRIER barrier {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	commandList->ResourceBarrier(1, &barrier);
	return intermediateResource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, int32_t width, int32_t height) {

	// 生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc {};
	resourceDesc.Width = width; // Textureの幅
	resourceDesc.Height = height; // Textureの高さ
	resourceDesc.MipLevels = 1; // mipmapの数
	resourceDesc.DepthOrArraySize = 1; // 奥行き or 配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // DepthStencilとして利用可能なフォーマット
	resourceDesc.SampleDesc.Count = 1;// サンプリングカウント。1固定。
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // DepthStencilとして使う通知

	// 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作る

	// 深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue {};
	depthClearValue.DepthStencil.Depth = 1.0f; // 1.0f(最大値)でクリア
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//フォーマット。Resourceと合わせる

	// Resourceの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties, // Heapの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし。
		&resourceDesc, // Resourceの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度値を書き込む状態にしておく
		&depthClearValue, // Clear最適値
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));
	return resource;
}

#pragma endregion

#pragma region AssetLoading

MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
{
	//----中で必要となる変数の宣言----
	MaterialData materialData; // 構築するMaterialData
	std::string line; // ファイルから読んだ1行を格納するもの

	//----ファイルを開く----
	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // とりあえず開かなかったら止める

	//----ファイルを読み込んで、MaterialDataを構築する----
	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		// identiffierに応じた処理
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			// 連結してファイルパスにする
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}

	//----MaterialDataを返す----
	return materialData;
}

ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename)
{
	//----必要な変数宣言----
	ModelData modelData; // 構築するModelData
	std::vector<Vector4> positions; // 位置
	std::vector<Vector3> normals; // 法線
	std::vector<Vector2> texcoords; // テクスチャ座標
	std::string line; // ファイルから読んだ1行を格納するもの

	//----ファイルを開く----
	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // とりあえず開けなかったら止める

	//----ファイルを読み込んで、ModelDataを構築していく----
	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier; // 先頭の識別子を読む

		// identifierに応じた処理
		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.x *= -1.0f;
			position.w = 1.0f;
			positions.push_back(position);
		} else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;
			texcoords.push_back(texcoord);
		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normal.x *= -1.0f;
			normals.push_back(normal);
		} else if (identifier == "f") {

			VertexData triangle[3];
			// 面は三角形限定。その他は未対応
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;
				// 頂点の要素へのIndexは「位置/UV/法線」で格納されているので、分解してIndexを取得する
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];
				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/'); // /区切りでインデックスを読んでいく
					elementIndices[element] = std::stoi(index);
				}
				// 要素へのIndexから、実際の要素の値を取得して、頂点を構築する
				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];
				triangle[faceVertex] = { position,texcoord,normal };
			}
			// 頂点を逆順で登録することで、回り順を逆にする
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
		} else if (identifier == "mtllib") {
			// materialTemplateLibraryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename;
			// 基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイル名を渡す
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}

	//----ModelDataを返す----
	return modelData;
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

#pragma region ウィンドウを作ろう

	// ポインタ
	WinApp* winApp = nullptr;

	// WindowsAPIの初期化
	winApp = new WinApp();
	winApp->Initialize();

#pragma endregion

	D3DResourceLeakChecker leakCheck;

#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		// デバックレイヤーを有効化する
		debugController->EnableDebugLayer();
		// さらにGPU側でもチェックを行うようにする
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif

#pragma region DirectX12初期化

	/**************************************************
	* DXGIファクトリーの生成
	**************************************************/

	// DXGIファクトリーの生成
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory = nullptr;
	// HRESULTはWindows系のエラーコードであり、
	// 関数が成功したかどうかをSUCCEEDEDマクロで判定できる
	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
	// 初期化の根本的な部分でエラーが出た場合はプログラムが間違っているか、どうにもできない場合が多いのでassertにしておく
	assert(SUCCEEDED(hr));

	/**************************************************
	 * 使用するGPU (アダプタ) の選定
	 **************************************************/

	 // 使用するアダプタ用の変数。最初にnullptrを入れておく
	Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter = nullptr;
	// 良い順にアダプタを頼む
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i,
		DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) !=
		DXGI_ERROR_NOT_FOUND; ++i) {
		// アダプタの情報を取得する
		DXGI_ADAPTER_DESC3 adapterDesc {};
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr)); // 取得できないのは一大事
		// ソフトウェアアダプタでなければ採用！
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			// 採用したアダプタの情報をログに出力。wstringの方なので注意
			Log(logStream, ConvertString(std::format(L"Use Adapater:{}\n", adapterDesc.Description)));
			break;
		}
		useAdapter = nullptr; // ソフトウェアアダプタの場合は見なかったことにする
	}
	// 適切なアダプタが見つからなかったので起動できない
	assert(useAdapter != nullptr);

	/**************************************************
	 * D3D12Deviceの生成
	 **************************************************/

	Microsoft::WRL::ComPtr<ID3D12Device> device = nullptr;
	// 機能レベルとログ出力用の文字列
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,D3D_FEATURE_LEVEL_12_1,D3D_FEATURE_LEVEL_12_0
	};
	const char* featureLevelStrings[] = { "12.2", "12.1", "12.0" };
	// 高い順に生成できるか試していく
	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		// 採用したアダプターでデバイスを生成
		hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&device));
		// 指定した機能レベルでデバイスを生成できたかを確認
		if (SUCCEEDED(hr)) {
			// 生成できたのでログ出力を行ってループを抜ける
			Log(logStream, std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
			break;
		}
	}
	// デバイスの生成がうまくいかなかったので起動できない
	assert(device != nullptr);
	Log(logStream, "Complete create D3D12Device!!!\n"); // 初期化完了のログを出す

#ifdef _DEBUG
	ID3D12InfoQueue* infoQueue = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		// ヤバイエラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		// エラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		// 警告時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		// 抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] = {
			// Windows11でのDXGIデバックレイヤーとDX12デバックレイヤーの相互作用バグによるエラーメッセージ
			// https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};
		// 抑制するレベル
		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter {};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		// 指定したメッセージの表示を抑制する
		infoQueue->PushStorageFilter(&filter);

		// 解放
		infoQueue->Release();
	}
#endif

#pragma endregion

#pragma region 画面の色を変える

	/**************************************************
	* CommandQueueを生成する
	**************************************************/

	// コマンドキューを生成する
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc {};
	hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));
	// コマンドキューの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	/**************************************************
	* CommandListを生成する
	**************************************************/

	// コマンドアロケータを生成する
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	// コマンドアロケータの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	// コマンドリストを生成する
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = nullptr;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));
	// コマンドリストの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	/**************************************************
	* SwapChainを生成する
	**************************************************/

	// スワップチェーンを生成する
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc {};
	swapChainDesc.Width = WinApp::kClientWidth; // 画面の幅。ウィンドウのクライアント領域を同じものにしておく
	swapChainDesc.Height = WinApp::kClientHeight; // 画面の高さ。ウィンドウのクライアント領域を同じものにしておく
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 色の形式
	swapChainDesc.SampleDesc.Count = 1; // マルチサンプル
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画のターゲットとして利用する
	swapChainDesc.BufferCount = 2; // ダブルバッファ
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // モニタにうつしたら、中身を破棄
	// コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue.Get(), winApp->GetHwnd(), &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(swapChain.GetAddressOf()));
	assert(SUCCEEDED(hr));

	/**************************************************
	* DescriptorHeapを生成する
	**************************************************/

	// RTV用のヒープでディスクリプタの数は2。RTVはShder内で触るものではないので、ShaderVisibleはfalse
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);

	// SRV用のヒープでディスクリプタの数は128。SRVはShader内で触るものなので、ShaderVisibleはtrue
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

	// DescriptorSizeを取得しておく
	const uint32_t desriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const uint32_t desriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const uint32_t desriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	/**************************************************
	* SwapChainからResourceを引っ張ってくる
	**************************************************/

	// SwapChainからResourceを引っ張ってくる
	Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources[2] = { nullptr };
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
	// うまく取得できなければ起動できない
	assert(SUCCEEDED(hr));
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	assert(SUCCEEDED(hr));

	/**************************************************
	* RTVの作る
	**************************************************/

	// RTVの設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 出力結果をSRGBに変換して書き込む
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2dテクスチャとして書き込む
	// ディスクリプタの先頭を取得する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = GetCPUDescriptorHandle(rtvDescriptorHeap, desriptorSizeRTV, 0);
	// RTVを2つ作るのでディスクリプタを2つ用意
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
	// まず1つ目を作る。1つ目は最初のところに作る。作る場所をこちらで指定してあげる必要がある
	rtvHandles[0] = rtvStartHandle;
	device->CreateRenderTargetView(swapChainResources[0].Get(), &rtvDesc, rtvHandles[0]);
	// 2つ目のディスクリプタハンドルを得る（自力で）
	rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	// 2つ目を作る
	device->CreateRenderTargetView(swapChainResources[1].Get(), &rtvDesc, rtvHandles[1]);

#pragma endregion

#pragma region 音楽

	Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
	IXAudio2MasteringVoice* masterVoice;

	// XAudioエンジンのインスタンスを生成
	HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);

	// マスターボイスを生成
	result = xAudio2->CreateMasteringVoice(&masterVoice);

	// 音声読み込み
	SoundData soundData1 = SoundLoadWave("resources/audio/Alarm02.wav");

	// 音楽再生
	SoundPlayWave(xAudio2.Get(), soundData1);

#pragma endregion

#pragma region 入力デバイス

	// ポインタ
	Input* input = nullptr;

	// 入力の初期化
	input = new Input();
	input->Initialize(winApp);

#pragma endregion

#pragma region FenceとEventを生成

	// 初期値0でFenceを作る
	Microsoft::WRL::ComPtr<ID3D12Fence> fence = nullptr;
	uint64_t fenceValue = 0;
	hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));

	// FenceのSignalを持つためのイベントを作成する
	HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent != nullptr);

#pragma endregion

#pragma region ImGui初期化

#ifdef USE_IMGUI
	// ImGuiの初期化。詳細はさして重要ではないので解説は省略する
	// こういうもんである
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(winApp->GetHwnd());
	ImGui_ImplDX12_Init(device.Get(),
		swapChainDesc.BufferCount,
		rtvDesc.Format,
		srvDescriptorHeap.Get(),
		srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
#endif

#pragma endregion

#pragma region Shader Compile

	// dxcCompilerを初期化
	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));

	// 現時点でincludeはしないが、includeに対応するための設定を行っておく
	IDxcIncludeHandler* includeHandler = nullptr;
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));

#pragma endregion

#pragma region PSO

	//----RootSignatureを生成する----

	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature {};
	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//----DescriptorRange----
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // 0から始まる
	descriptorRange[0].NumDescriptors = 1; // 数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

	//----RootParameter----

	// RootParameter作成。PixelShaderのMaterialとVertexShaderのTransform
	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号0とバインド

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // VertexShaderで使う
	rootParameters[1].Descriptor.ShaderRegister = 0; // レジスタ番号0を使う

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange); // Tableで利用する数

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[3].Descriptor.ShaderRegister = 1; // レジスタ番号1を使う

	descriptionRootSignature.pParameters = rootParameters; // ルートパラメータ配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters); // 配列の長さ

	//----Sampler----
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 0~1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0; // レジスタ番号0を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	// シリアライズしてバイナリにする
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Log(logStream, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	// バイナリを元に生成
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	hr = device->CreateRootSignature(0,
		signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));

	//----InputLayoutの設定を行う----

	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc {};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	//----BlendStateの設定を行う----

	// BlendStateの設定
	D3D12_BLEND_DESC blendDesc {};
	// すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask =
		D3D12_COLOR_WRITE_ENABLE_ALL;

	//----RasterizerStateの設定を行う----

	// RasterzerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc {};
	// 裏面（時計回り）を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	//----ShaderをCompileする----

	// Shaderをコンパイルする
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = CompileShader(L"resources/shaders/Object3D.VS.hlsl",
		L"vs_6_0", dxcUtils, dxcCompiler, includeHandler, logStream);
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = CompileShader(L"resources/shaders/Object3D.PS.hlsl",
		L"ps_6_0", dxcUtils, dxcCompiler, includeHandler, logStream);
	assert(pixelShaderBlob != nullptr);

	//----PSOを生成する----

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc {};
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get(); // RootSignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc; // InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() }; // VertexShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() }; // PixelShader
	graphicsPipelineStateDesc.BlendState = blendDesc; // BlenState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc; // RasterizerState
	// 書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトポロジ（形状）のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定（気にしなくて良い）
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	//----DepthStencilStateの設定----

	// DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc {};
	// Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	// 書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	// 比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 実際に生成
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,
		IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));

#pragma endregion

#pragma region 深度・ステンシル

	//----Resource----

	// DepthStencilTextureをウィンドウのサイズで作成
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource = CreateDepthStencilTextureResource(device, WinApp::kClientWidth, WinApp::kClientHeight);

	//----DepthStencilView(DSV)----

	//DSV用のヒープでディスクリプタの数は1。DSVはShader内で触るものではないので、ShaderVisibleははfalse
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	// DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc {};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // Format。 基本的にはResourceに合わせる
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // 2dTexture
	// DSVHeapの先頭にDSVをつくる
	device->CreateDepthStencilView(depthStencilResource.Get(), &dsvDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

#pragma endregion

#pragma region ViewportとScissor

	// ビューポート
	D3D12_VIEWPORT viewport {};
	// クラインアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = WinApp::kClientWidth;
	viewport.Height = WinApp::kClientHeight;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	// シザー矩形
	D3D12_RECT scissorRect {};
	// 基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0;
	scissorRect.right = WinApp::kClientWidth;
	scissorRect.top = 0;
	scissorRect.bottom = WinApp::kClientHeight;

#pragma endregion

#pragma region Resources: 3D Object (ModelData)

	/**************************************************
	* 頂点データの作成とビュー
	**************************************************/

	// モデル読み込み
	ModelData modelData = LoadObjFile("resources/models/plane", "plane.obj");
	//----VertexResourceを生成する----
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = CreateBufferResource(device, sizeof(VertexData) * modelData.vertices.size());

	//----VertexBufferViewを生成する----

	// 頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView {};
	// リソースの先頭のアドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点6つ分のサイズ
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
	// 1頂点あたりのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	//----Resourceにデータを書き込む----

	VertexData* vertexData = nullptr;
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData)); // 書き込むためのアドレスを取得
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size()); // 頂点データをリソースにコピー

	/**************************************************
	* Transform
	**************************************************/

	// Transform変数を作る
	Transform transform { {1.0f,1.0f,1.0f},{0.0f,3.14f,0.0f},{0.0f,0.0f,0.0f} };

	// camera変数を作る
	Transform cameraTransform { {1.0f, 1.0f, 1.0f},{0.0f, 0.0f, 0.0f},{0.0f,0.0f,-10.0f} };

	/**************************************************
	* TransformationMatrix用のResourceを作る
	**************************************************/

	// WVP用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource = CreateBufferResource(device, sizeof(TransformationMatrix));
	// データを書き込む
	TransformationMatrix* wvpData = nullptr;
	// 書き込むためのアドレスを取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	// 単位行列を書き込んでおく
	wvpData->WVP = MakeIdentity4x4();
	wvpData->World = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);

	/**************************************************
	* Material用のResourceを作る
	**************************************************/

	// マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = CreateBufferResource(device, sizeof(Material));
	// マテリアルにデータを書き込む
	Material* materialData = nullptr;
	// 書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	// 今回は赤を書き込んでみる
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = false;
	materialData->uvTransform = MakeIdentity4x4();

	/**************************************************
	* ランバート反射
	**************************************************/

	// マテリアルリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> resourceDirectionalLight = CreateBufferResource(device, sizeof(DirectionalLight));

	// directionalLight用のリソースを作る
	DirectionalLight* directionalLightData = nullptr;

	resourceDirectionalLight->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));

	// デフォルト値
	directionalLightData->color = { 1.0f,1.0f,1.0f,1.0f };
	directionalLightData->direction = { 0.0f,-1.0f,0.0f };
	directionalLightData->intensity = 1.0f;

#pragma endregion

#pragma region Resources: 3D Object (Triangle)

	/**************************************************
	* 頂点データの作成とビュー
	**************************************************/

	//----VertexResourceを生成する----

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceTriangle = CreateBufferResource(device, sizeof(VertexData) * 3);

	//----VertexBufferViewを生成する----

	// 頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewTriangle {};
	// リソースの先頭のアドレスから使う
	vertexBufferViewTriangle.BufferLocation = vertexResourceTriangle->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点6つ分のサイズ
	vertexBufferViewTriangle.SizeInBytes = sizeof(VertexData) * 3;
	// 1頂点あたりのサイズ
	vertexBufferViewTriangle.StrideInBytes = sizeof(VertexData);

	//----Resourceにデータを書き込む----

	// 頂点リソースにデータを書き込む
	VertexData* vertexDataTriangle = nullptr;
	// 書き込むためのアドレスを取得
	vertexResourceTriangle->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataTriangle));
	// 左下
	vertexDataTriangle[0].position = { -0.5f,-0.5f,0.0f,1.0f };
	vertexDataTriangle[0].texcoord = { 0.0f,1.0f };
	vertexDataTriangle[0].normal = { 0.0f, 0.0f,-1.0f };
	// 上
	vertexDataTriangle[1].position = { 0.0f,0.5f,0.0f,1.0f };
	vertexDataTriangle[1].texcoord = { 0.5f,0.0f };
	vertexDataTriangle[1].normal = { 0.0f, 0.0f,-1.0f };
	// 右下
	vertexDataTriangle[2].position = { 0.5f,-0.5f,0.0f,1.0f };
	vertexDataTriangle[2].texcoord = { 1.0f,1.0f };
	vertexDataTriangle[2].normal = { 0.0f, 0.0f,-1.0f };

	/**************************************************
	* IndexResource
	**************************************************/

	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceTriangle = CreateBufferResource(device, sizeof(uint32_t) * 3);

	D3D12_INDEX_BUFFER_VIEW indexBufferViewTriangle {};
	indexBufferViewTriangle.BufferLocation = indexResourceTriangle->GetGPUVirtualAddress();
	indexBufferViewTriangle.SizeInBytes = sizeof(uint32_t) * 3;
	indexBufferViewTriangle.Format = DXGI_FORMAT_R32_UINT;

	uint32_t* indexDataTriangle = nullptr;
	indexResourceTriangle->Map(0, nullptr, reinterpret_cast<void**>(&indexDataTriangle));
	indexDataTriangle[0] = 0;
	indexDataTriangle[1] = 1;
	indexDataTriangle[2] = 2;

	/**************************************************
	* Transform
	**************************************************/

	// Transform変数を作る
	Transform transformTriangle { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	// camera変数を作る
	Transform cameraTransformTriangle { {1.0f, 1.0f, 1.0f},{0.0f, 0.0f, 0.0f},{0.0f,0.0f,-5.0f} };

	/**************************************************
	* TransformationMatrix用のResourceを作る
	**************************************************/

	// WVP用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResourceTriangle = CreateBufferResource(device, sizeof(TransformationMatrix));
	// データを書き込む
	TransformationMatrix* wvpDataTriangle = nullptr;
	// 書き込むためのアドレスを取得
	wvpResourceTriangle->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataTriangle));
	// 単位行列を書き込んでおく
	wvpDataTriangle->WVP = MakeIdentity4x4();
	wvpDataTriangle->World = MakeAffineMatrix(transformTriangle.scale, transformTriangle.rotate, transformTriangle.translate);

	/**************************************************
	* Material用のResourceを作る
	**************************************************/

	// マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceTriangle = CreateBufferResource(device, sizeof(Material));
	// マテリアルにデータを書き込む
	Material* materialDataTriangle = nullptr;
	// 書き込むためのアドレスを取得
	materialResourceTriangle->Map(0, nullptr, reinterpret_cast<void**>(&materialDataTriangle));
	// 今回は赤を書き込んでみる
	materialDataTriangle->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialDataTriangle->enableLighting = false;
	materialDataTriangle->uvTransform = MakeIdentity4x4();

	/**************************************************
	* ランバート反射
	**************************************************/

	// マテリアルリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> resourceDirectionalLightTriangle = CreateBufferResource(device, sizeof(DirectionalLight));

	// directionalLight用のリソースを作る
	DirectionalLight* directionalLightDataTriangle = nullptr;

	resourceDirectionalLightTriangle->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightDataTriangle));

	// デフォルト値
	directionalLightDataTriangle->color = { 1.0f,1.0f,1.0f,1.0f };
	directionalLightDataTriangle->direction = { 0.0f,-1.0f,0.0f };
	directionalLightDataTriangle->intensity = 1.0f;

#pragma endregion

#pragma region Resources: 3D Object (Sphere)

	// 球の分割数と頂点数計算
	uint32_t kSubdivision = 16;
	uint32_t vertexCount = (kSubdivision + 1) * (kSubdivision + 1);
	uint32_t indexCount = kSubdivision * kSubdivision * 6;

	/**************************************************
	* 頂点データの作成とビュー
	**************************************************/

	// 頂点リソースの作成
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSphere = CreateBufferResource(device, sizeof(VertexData) * vertexCount);

	// 頂点バッファビューの作成
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSphere {};
	vertexBufferViewSphere.BufferLocation = vertexResourceSphere->GetGPUVirtualAddress();
	vertexBufferViewSphere.SizeInBytes = sizeof(VertexData) * vertexCount;
	vertexBufferViewSphere.StrideInBytes = sizeof(VertexData);

	// 頂点データのマップと生成
	VertexData* vertexDataSphere = nullptr;
	vertexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSphere));

	const float kLonEvery = std::numbers::pi_v<float> *2.0f / float(kSubdivision);
	const float kLatEvery = std::numbers::pi_v<float> / float(kSubdivision);

	for (uint32_t lat = 0; lat <= kSubdivision; ++lat) {
		float latAngle = -std::numbers::pi_v<float> / 2.0f + lat * kLatEvery;
		for (uint32_t lon = 0; lon <= kSubdivision; ++lon) {
			float lonAngle = lon * kLonEvery;

			uint32_t index = lat * (kSubdivision + 1) + lon;

			Vector3 pos = {
				std::cosf(latAngle) * std::cosf(lonAngle),
				std::sinf(latAngle),
				std::cosf(latAngle) * std::sinf(lonAngle)
			};

			vertexDataSphere[index] = {
				{ pos.x, pos.y, pos.z, 1.0f },
				{ float(lon) / kSubdivision, 1.0f - float(lat) / kSubdivision },
				{ pos.x, pos.y, pos.z }
			};
		}
	}

	/**************************************************
	* IndexResource
	**************************************************/

	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSphere = CreateBufferResource(device, sizeof(uint32_t) * indexCount);

	D3D12_INDEX_BUFFER_VIEW indexBufferViewSphere {};
	indexBufferViewSphere.BufferLocation = indexResourceSphere->GetGPUVirtualAddress();
	indexBufferViewSphere.SizeInBytes = sizeof(uint32_t) * indexCount;
	indexBufferViewSphere.Format = DXGI_FORMAT_R32_UINT;

	uint32_t* indexDataSphere = nullptr;
	indexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSphere));

	uint32_t index = 0;
	for (uint32_t lat = 0; lat < kSubdivision; ++lat) {
		for (uint32_t lon = 0; lon < kSubdivision; ++lon) {

			uint32_t a = lat * (kSubdivision + 1) + lon;
			uint32_t b = (lat + 1) * (kSubdivision + 1) + lon;
			uint32_t c = lat * (kSubdivision + 1) + (lon + 1);
			uint32_t d = (lat + 1) * (kSubdivision + 1) + (lon + 1);

			// 三角形1: A-B-C
			indexDataSphere[index++] = a;
			indexDataSphere[index++] = b;
			indexDataSphere[index++] = c;

			// 三角形2: C-B-D
			indexDataSphere[index++] = c;
			indexDataSphere[index++] = b;
			indexDataSphere[index++] = d;
		}
	}

	/**************************************************
	* Transform
	**************************************************/

	Transform transformSphereInit;

	// SphereTransform変数を作る
	Transform transformSphere { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	transformSphereInit = transformSphere;

	// SphereCamera変数を作る
	Transform cameraTransformSphere { {1.0f, 1.0f, 1.0f},{0.0f, 0.0f, 0.0f},{0.0f,0.0f,-10.0f} };

	/**************************************************
	* TransformationMatrix用のResourceを作る
	**************************************************/

	// 1つ分のサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResourceSphere = CreateBufferResource(device, sizeof(TransformationMatrix));
	// データを書き込む
	TransformationMatrix* wvpDataSphere = nullptr;
	// 書き込むためのアドレスを取得
	wvpResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataSphere));
	// 単位行列を書き込んでおく
	wvpDataSphere->WVP = MakeIdentity4x4();
	wvpDataSphere->World = MakeAffineMatrix(transformSphere.scale, transformSphere.rotate, transformSphere.translate);

	/**************************************************
	* Material用のResourceを作る
	**************************************************/

	// マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSphere = CreateBufferResource(device, sizeof(Material));
	Material* materialDataSphere = nullptr;
	materialResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSphere));
	materialDataSphere->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialDataSphere->enableLighting = false;
	materialDataSphere->uvTransform = MakeIdentity4x4();

	/**************************************************
	* ランバート反射
	**************************************************/

	// マテリアルリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> resourceDirectionalLightSphere = CreateBufferResource(device, sizeof(DirectionalLight));

	// directionalLight用のリソースを作る
	DirectionalLight* directionalLightDataSphere = nullptr;

	resourceDirectionalLightSphere->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightDataSphere));

	// デフォルト値
	directionalLightDataSphere->color = { 1.0f,1.0f,1.0f,1.0f };
	directionalLightDataSphere->direction = { 0.0f,-1.0f,0.0f };
	directionalLightDataSphere->intensity = 1.0f;

#pragma endregion

#pragma region Resources: 2D Object (Sprite)

	/**************************************************
	* 頂点データの作成とビュー
	**************************************************/

	// ----VertexResourceを生成する----
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite = CreateBufferResource(device, sizeof(VertexData) * 4);

	// ----VertexBufferViewを生成する----

	// 頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite {};
	// リソースの先頭のアドレスから使う
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点6つ分のサイズ
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 4;
	// 1頂点あたりのサイズ
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

	// ----Resourceにデータを書き込む----

	VertexData* vertexDataSprite = nullptr;
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));
	// 1枚目の三角形
	vertexDataSprite[0].position = { 0.0f,360.0f,0.0f,1.0f }; // 左下
	vertexDataSprite[0].texcoord = { 0.0f,1.0f };
	vertexDataSprite[0].normal = { 0.0f,0.0f,-1.0f };

	vertexDataSprite[1].position = { 0.0f,0.0f,0.0f,1.0f }; // 左上
	vertexDataSprite[1].texcoord = { 0.0f,0.0f };
	vertexDataSprite[1].normal = { 0.0f,0.0f,-1.0f };

	vertexDataSprite[2].position = { 640.0f,360.0f,0.0f,1.0f }; // 右下
	vertexDataSprite[2].texcoord = { 1.0f,1.0f };
	vertexDataSprite[2].normal = { 0.0f,0.0f,-1.0f };

	vertexDataSprite[3].position = { 640.0f,0.0f,0.0f,1.0f }; // 右上
	vertexDataSprite[3].texcoord = { 1.0f,0.0f };
	vertexDataSprite[3].normal = { 0.0f,0.0f,-1.0f };

	/**************************************************
	* IndexResource
	**************************************************/

	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite = CreateBufferResource(device, sizeof(uint32_t) * 6);

	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite {};
	// リソースの先頭のアドレスから使う
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	// 使用するリソースのサイズはインデックス6つ分のサイズ
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
	// インデックスはuint32_tとする
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

	// インデックスリソースにデータを書き込む
	uint32_t* indexDataSprite = nullptr;
	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
	indexDataSprite[0] = 0;
	indexDataSprite[1] = 1;
	indexDataSprite[2] = 2;
	indexDataSprite[3] = 1;
	indexDataSprite[4] = 3;
	indexDataSprite[5] = 2;

	/**************************************************
	* Transform
	**************************************************/

	// SpriteTransform変数を作る
	Transform transformSprite { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	Transform uvTransformSprite { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	/**************************************************
	* TransformationMatrix用のResourceを作る
	**************************************************/

	// Sprite用のTransformationMatrix用のリソースを作る。Matrix4×4 1つ分のサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResourceSprite = CreateBufferResource(device, sizeof(TransformationMatrix));
	// データを書き込む
	TransformationMatrix* wvpDataSprite = nullptr;
	// 書き込むためのアドレスを取得
	wvpResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataSprite));
	// 単位行列を書きこんでおく
	wvpDataSprite->WVP = MakeIdentity4x4();
	wvpDataSprite->World = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);

	/**************************************************
	* Material用のResourceを作る
	**************************************************/

	// マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite = CreateBufferResource(device, sizeof(Material));
	Material* materialDataSprite = nullptr;
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
	materialDataSprite->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	// Lightingを有効にする
	materialDataSprite->enableLighting = false;
	materialDataSprite->uvTransform = MakeIdentity4x4();

#pragma endregion

#pragma region テクスチャを読み込んで使う

	// Textureを読んで転送する
	DirectX::ScratchImage mipImages = LoadTexture("resources/textures/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource = CreateTextureResource(device, metadata);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = UploadTextureData(textureResource, mipImages, device, commandList);

	// 2枚目のTextureを読んで転送する
	DirectX::ScratchImage mipImages2 = LoadTexture(modelData.material.textureFilePath);
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2 = CreateTextureResource(device, metadata2);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource2 = UploadTextureData(textureResource2, mipImages2, device, commandList);

#pragma endregion

#pragma region ShaderResourceViewを作る

	// metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	// SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap, desriptorSizeSRV, 1);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap, desriptorSizeSRV, 1);
	// SRVの生成
	device->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);

	// 2枚目のmetaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2 {};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	// 2枚目のSRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = GetCPUDescriptorHandle(srvDescriptorHeap, desriptorSizeSRV, 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = GetGPUDescriptorHandle(srvDescriptorHeap, desriptorSizeSRV, 2);
	// 2枚目のSRVの生成
	device->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);

#pragma endregion

#pragma region 状態管理（フラグなど）

	//平行光源の切り替え	
	bool useLighting = false;
	bool useLightingTriangle = false;
	bool useLightingSphere = false;

	bool showModelData = true;
	bool showTriangle = false;
	bool showSphere = false;
	bool showSprite = false;

#pragma endregion

#pragma region メインループ

	MSG msg {};
	// ウィンドウの×ボタンが押されるまでループ
	while (msg.message != WM_QUIT) {
		// Windowにメッセージが来てたら最優先で処理させる
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {
			// ゲームの処理

			// 入力の更新
			input->Update();

			if (input->PushKey(DIK_W))
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
			}

		#ifdef USE_IMGUI
			// フレームの開始
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
		#endif

		#ifdef USE_IMGUI
			ImGui::Begin("Settings");

			// 対象選択
			static int current = 0;
			const char* targets[] = { "ModelData","Sphere", "Sprite","Triangle" };
			ImGui::Combo("Target", &current, targets, IM_ARRAYSIZE(targets));

			ImGui::Separator();

			if (current == 0) {

				ImGui::Checkbox("Show ModelData", &showModelData);

				ImGui::Separator();
				ImGui::InputFloat3("CameraTranslate", &cameraTransform.translate.x);
				ImGui::SliderAngle("CameraRotateX", &cameraTransform.rotate.x);
				ImGui::SliderAngle("CameraRotateY", &cameraTransform.rotate.y);
				ImGui::SliderAngle("CameraRotateZ", &cameraTransform.rotate.z);
				ImGui::SliderAngle("RotateX", &transform.rotate.x);
				ImGui::SliderAngle("RotateY", &transform.rotate.y);
				ImGui::SliderAngle("RotateZ", &transform.rotate.z);
				ImGui::Separator();
				ImGui::ColorEdit3("Color", &materialData->color.x);
				ImGui::Separator();
				ImGui::Checkbox("enableLighting", &useLighting);
				useLighting ? materialData->enableLighting = true : materialData->enableLighting = false;
				ImGui::Separator();
				ImGui::ColorEdit3("Light Color", &directionalLightData->color.x);
				ImGui::SliderFloat3("Light Direction", &directionalLightData->direction.x, -1.0f, 1.0f);
				ImGui::SliderFloat("Light Intensity", &directionalLightData->intensity, 0.0f, 5.0f);

			} else if (current == 1) {

				ImGui::Checkbox("Show Sphere", &showSphere);

				// ===== Sphere =====
				ImGui::Text("Transform");
				ImGui::SliderFloat3("Scale", &transformSphere.scale.x, 0.1f, 5.0f);
				ImGui::SliderFloat3("Rotate", &transformSphere.rotate.x, -3.14f, 3.14f);
				ImGui::SliderFloat3("Translate", &transformSphere.translate.x, -5.0f, 5.0f);
				if (ImGui::Button("Reset transformSphere")) {
					transformSphere = transformSphereInit;
				}

				// ===== Material =====
				ImGui::Separator();
				ImGui::Text("Material");
				ImGui::ColorEdit3("Color", &materialDataSphere->color.x);

				ImGui::Separator();
				ImGui::Text("Camera");
				ImGui::SliderFloat3("Camera Position", &cameraTransformSphere.translate.x, -20.0f, 20.0f);

				ImGui::Separator();
				ImGui::Text("Directional Light");
				ImGui::ColorEdit3("Light Color", &directionalLightDataSphere->color.x);
				ImGui::SliderFloat3("Light Direction", &directionalLightDataSphere->direction.x, -1.0f, 1.0f);
				ImGui::SliderFloat("Light Intensity", &directionalLightDataSphere->intensity, 0.0f, 5.0f);

				ImGui::Separator();
				ImGui::Text("CheckBox");
				ImGui::Checkbox("enableLighting", &useLightingSphere);
				useLightingSphere ? materialDataSphere->enableLighting = true : materialDataSphere->enableLighting = false;
			} else if (current == 2) {

				ImGui::Checkbox("Show Sprite", &showSprite);

				// ===== Sprite =====
				ImGui::Text("Transform");
				ImGui::SliderFloat3("Scale", &transformSprite.scale.x, 0.1f, 5.0f);
				ImGui::SliderFloat3("Rotate", &transformSprite.rotate.x, -3.14f, 3.14f);
				ImGui::SliderFloat3("Translate", &transformSprite.translate.x, -640.0f, 640.0f);

				// ===== Material =====
				ImGui::Separator();
				ImGui::Text("Material");
				ImGui::ColorEdit3("Color", &materialDataSprite->color.x);

				ImGui::Separator();
				ImGui::Text("UVTransform");
				ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
				ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
				ImGui::SliderAngle("UVRotate", &uvTransformSprite.rotate.z);
			} else {

				ImGui::Checkbox("Show Triangle", &showTriangle);

				// ===== Triangle =====
				ImGui::Text("Transform");
				ImGui::SliderFloat3("Scale", &transformTriangle.scale.x, 0.1f, 5.0f);
				ImGui::SliderFloat3("Rotate", &transformTriangle.rotate.x, -3.14f, 3.14f);
				ImGui::SliderFloat3("Translate", &transformTriangle.translate.x, -5.0f, 5.0f);

				// ===== Material =====
				ImGui::Separator();
				ImGui::Text("Material");
				ImGui::ColorEdit3("Color", &materialDataTriangle->color.x);

				ImGui::Separator();
				ImGui::Text("Camera");
				ImGui::SliderFloat3("Camera Position", &cameraTransformTriangle.translate.x, -10.0f, 10.0f);

				ImGui::Separator();
				ImGui::Text("Directional Light");
				ImGui::ColorEdit3("Light Color", &directionalLightDataTriangle->color.x);
				ImGui::SliderFloat3("Light Direction", &directionalLightDataTriangle->direction.x, -1.0f, 1.0f);
				ImGui::SliderFloat("Light Intensity", &directionalLightDataTriangle->intensity, 0.0f, 5.0f);

				ImGui::Separator();
				ImGui::Text("CheckBox");
				ImGui::Checkbox("enableLighting", &useLightingTriangle);
				useLightingTriangle ? materialDataTriangle->enableLighting = true : materialDataTriangle->enableLighting = false;
			}

			ImGui::End();
		#endif

		#pragma region ワールド・ビュー・プロジェクション行列計算: 3D Object (ModelData)

			//transform.rotate.y += 0.01f;
			Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
			Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
			Matrix4x4 viewMatrix = Inverse(cameraMatrix);
			Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClientHeight), 0.1f, 100.0f);
			Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
			wvpData->WVP = worldViewProjectionMatrix;
			wvpData->World = worldMatrix;

		#pragma endregion

		#pragma region ワールド・ビュー・プロジェクション行列計算: 3D Object (Triangle)

			//transform.rotate.y += 0.01f;
			Matrix4x4 worldMatrixTriangle = MakeAffineMatrix(transformTriangle.scale, transformTriangle.rotate, transformTriangle.translate);
			Matrix4x4 cameraMatrixTriangle = MakeAffineMatrix(cameraTransformTriangle.scale, cameraTransformTriangle.rotate, cameraTransformTriangle.translate);
			Matrix4x4 viewMatrixTriangle = Inverse(cameraMatrixTriangle);
			Matrix4x4 projectionMatrixTriangle = MakePerspectiveFovMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClientHeight), 0.1f, 100.0f);
			Matrix4x4 worldViewProjectionMatrixTriangle = Multiply(worldMatrixTriangle, Multiply(viewMatrixTriangle, projectionMatrixTriangle));
			wvpDataTriangle->WVP = worldViewProjectionMatrixTriangle;
			wvpDataTriangle->World = worldMatrixTriangle;

		#pragma endregion

		#pragma region ワールド・ビュー・プロジェクション行列計算: 3D Object (Sphere)

			transformSphere.rotate.y += 0.01f;
			Matrix4x4 worldMatrixSphere = MakeAffineMatrix(transformSphere.scale, transformSphere.rotate, transformSphere.translate);
			Matrix4x4 cameraMatrixSphere = MakeAffineMatrix(cameraTransformSphere.scale, cameraTransformSphere.rotate, cameraTransformSphere.translate);
			Matrix4x4 viewMatrixSphere = Inverse(cameraMatrixSphere);
			Matrix4x4 projectionMatrixSphere = MakePerspectiveFovMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClientHeight), 0.1f, 100.0f);
			Matrix4x4 worldViewProjectionMatrixSphere = Multiply(worldMatrixSphere, Multiply(viewMatrixSphere, projectionMatrixSphere));
			wvpDataSphere->WVP = worldViewProjectionMatrixSphere;
			wvpDataSphere->World = worldMatrixSphere;

		#pragma endregion

		#pragma region ワールド・ビュー・プロジェクション行列計算: 2D Object (Sprite)	

			// Sprite用のWorldViewProjectionMatrixを作る
			Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
			Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
			Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0.0f, 100.0f);
			Matrix4x4 worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));
			wvpDataSprite->WVP = worldViewProjectionMatrixSprite;
			wvpDataSprite->World = worldMatrixSprite;

			Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvTransformSprite.scale);
			uvTransformMatrix = Multiply(uvTransformMatrix, MakeRotateZMatrix(uvTransformSprite.rotate.z));
			uvTransformMatrix = Multiply(uvTransformMatrix, MakeTranslateMatrix(uvTransformSprite.translate));
			materialDataSprite->uvTransform = uvTransformMatrix;

		#pragma endregion

		#ifdef USE_IMGUI
			// ImGuiの内部コマンドを生成する
			ImGui::Render();
		#endif

		#pragma region 画面をクリアする

			/**************************************************
			* コマンドを積み込む
			**************************************************/

			// これから書き込むバックバッファのインデックスを取得
			UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

			//----TransitionBarrierを張る----

			// TransitionBarrierの設定
			D3D12_RESOURCE_BARRIER barrier {};
			// 今回のバリアはTransition
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			// Noneにしておく
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			// バリアを張る対象のリソース。現在のバックバッファに対して行う
			barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
			// 遷移前（現在）のResourceState
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			// 遷移後のResourceState
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			// TransitionBarrierを張る
			commandList->ResourceBarrier(1, &barrier);

			//----描画先を設定、指定した色でクリア（塗りつぶしを）する----

			// 描画先のRTVとDSVを設定する
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = GetCPUDescriptorHandle(dsvDescriptorHeap, desriptorSizeDSV, 0);

			// 描画先のRTVを設定する
			commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, nullptr);

			// 指定した色で画面全体をクリアする
			float clearColor[] = { 0.1f,0.25f,0.5f,1.0f }; // 青っぽい色。RGBAの順
			commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

			commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);

			// 指定した深度で画面全体をクリアする
			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			// 描画用のDescriptorHeapの設定
			ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap.Get() };
			commandList->SetDescriptorHeaps(1, descriptorHeaps);

			//----コマンドを積む----
			commandList->RSSetViewports(1, &viewport); // Viewportを設定
			commandList->RSSetScissorRects(1, &scissorRect); // Scissorを設定

			// RootSignatureを設定。PSOに設定しているけどベット設定が必要
			commandList->SetGraphicsRootSignature(rootSignature.Get());
			commandList->SetPipelineState(graphicsPipelineState.Get()); // PSOを設定

		#pragma region 描画: 3D Object (ModelData)

			if (showModelData) {

				commandList->IASetVertexBuffers(0, 1, &vertexBufferView); // VBVを設定
				// 形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えておけばいい
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				// マテリアルCBufferの場所を設定
				commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
				// wvp用のCBufferの場所を設定
				commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());
				// SRVのDescriptorTableの先頭を設定。2はrootParameter[2]である。
				commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
				commandList->SetGraphicsRootConstantBufferView(3, resourceDirectionalLight->GetGPUVirtualAddress());

				commandList->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);

			}

		#pragma endregion

		#pragma region 描画: 3D Object (Triangle)

			if (showTriangle) {

				commandList->IASetVertexBuffers(0, 1, &vertexBufferViewTriangle); // VBVを設定
				commandList->IASetIndexBuffer(&indexBufferViewTriangle); // IBVを設定

				// 形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えておけばいい
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				// マテリアルCBufferの場所を設定
				commandList->SetGraphicsRootConstantBufferView(0, materialResourceTriangle->GetGPUVirtualAddress());
				// wvp用のCBufferの場所を設定
				commandList->SetGraphicsRootConstantBufferView(1, wvpResourceTriangle->GetGPUVirtualAddress());
				// SRVのDescriptorTableの先頭を設定。2はrootParameter[2]である。
				commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
				commandList->SetGraphicsRootConstantBufferView(3, resourceDirectionalLightTriangle->GetGPUVirtualAddress());

				commandList->DrawIndexedInstanced(3, 1, 0, 0, 0);

			}

		#pragma endregion

		#pragma region 描画: 3D Object (Sphere)

			if (showSphere) {

				commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSphere); // VBVを設定
				commandList->IASetIndexBuffer(&indexBufferViewSphere); // IBVを設定
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				commandList->SetGraphicsRootConstantBufferView(0, materialResourceSphere->GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(1, wvpResourceSphere->GetGPUVirtualAddress());
				commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU2);
				commandList->SetGraphicsRootConstantBufferView(3, resourceDirectionalLightSphere->GetGPUVirtualAddress());

				commandList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
			}

		#pragma endregion

		#pragma region 描画: 2D Object (Sprite)

			if (showSprite) {

				// Spriteの描画。変更が必要なものだけ変更する
				commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite); // VBVを設定

				commandList->IASetIndexBuffer(&indexBufferViewSprite); // IBVを設定

				commandList->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress());
				// TransformationMatrixCBufferの場所を設定
				commandList->SetGraphicsRootConstantBufferView(1, wvpResourceSprite->GetGPUVirtualAddress());
				commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
				//描画!(DrawCall/ドローコール)6個のインデックスを使用し1つのインスタンスを描画。その他は当面0で良い
				commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
			}

		#pragma endregion


		#ifdef USE_IMGUI
			// 実際のcommandListのImGuiの描画コマンドを積む
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());
		#endif

		#pragma endregion

			//----画面表示できるようにする----

			// 画面に描く処理はすべて終わり、画面に映すので、状態を遷移
			// 今回はRenderTargetからPresentにする
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			// TransitionBarrierを張る
			commandList->ResourceBarrier(1, &barrier);

			//----CommandListを閉じる----

			// コマンドリストの内容を確定させる。すべてのコマンドを積んでからCloseすること
			hr = commandList->Close();
			assert(SUCCEEDED(hr));

			/**************************************************
			* コマンドをキックする
			**************************************************/

			// GPUにコマンドリストの実行を行わせる
			ID3D12CommandList* commandLists[] = { commandList.Get() };
			commandQueue->ExecuteCommandLists(1, commandLists);
			// GPUとOSに画面の交換を行うよう通知する
			swapChain->Present(1, 0);

			//----GPUにSignalを送る----

			// Fenceの値を更新
			fenceValue++;
			// GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
			commandQueue->Signal(fence.Get(), fenceValue);

			//----待機処理----

			// Fenceの値が指定したSignal値にたどり着いているか確認する
			// GetCompletedValueの初期値はFence作成時に渡した初期値
			if (fence->GetCompletedValue() < fenceValue)
			{
				// 指定したSignalにたどりついていないので、たどり着くまで待つようにイベントを設定する
				fence->SetEventOnCompletion(fenceValue, fenceEvent);
				// イベント待つ
				WaitForSingleObject(fenceEvent, INFINITE);
			}

			// 次のフレーム用のコマンドリストを準備
			hr = commandAllocator->Reset();
			assert(SUCCEEDED(hr));
			hr = commandList->Reset(commandAllocator.Get(), nullptr);
			assert(SUCCEEDED(hr));

		#pragma endregion

		}
	}

#pragma endregion

#pragma region Object解放

	// WindowsAPIの終了処理
	winApp->Finalize();

#ifdef USE_IMGUI
	// ImGuiの終了処理。詳細はさして重要ではないので解説は省略する。
	// こういうもんである。初期化と逆順に行う
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif

	CloseHandle(fenceEvent);

	// 入力解放
	delete input;
	input = nullptr;

	// XAudio2解放
	xAudio2.Reset();
	// 音声データ解放
	SoundUnload(&soundData1);

	// WindowsAPI解放
	delete winApp;
	winApp = nullptr;

#pragma endregion

	return 0;
}