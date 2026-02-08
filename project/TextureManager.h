#pragma once
#include <string>
#include <unordered_map>
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"

class DirectXCommon;
class SrvManager;

// テクスチャマネージャー
class TextureManager
{
public:
	// シングルトンインスタンスの取得
	static TextureManager *GetInstance();
	// 終了
	void Finalize();

	// 初期化
	void Initialize(DirectXCommon *dxCommon, SrvManager *srvManager);

	/// <summary>
	/// テクスチャファイルの読み込み
	/// </summary>
	/// <param name="filePath">テクスチャファイルのパス</param>
	void LoadTexture(const std::string &filePath);

	void ReleaseIntermediateResources();

	// SRVインデックスの開始番号
	uint32_t GetTextureIndexByFilePath(const std::string &filePath);

	// テクスチャ番号からGPUハンドルを取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string &filePath);

	void SetDirectXCommon(DirectXCommon *dxCommon);

	// メタデータを取得
	const DirectX::TexMetadata &GetMetaData(const std::string &filePath);

private:
	static TextureManager *instance;

	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(TextureManager &) = delete;
	TextureManager &operator=(TextureManager &) = delete;

private:
	// テクスチャ1枚分のデータ
	struct TextureData
	{
		DirectX::TexMetadata metadata;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
		uint32_t srvIndex;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
	};

	// テクスチャデータ
	std::unordered_map<std::string, TextureData> textureDatas;

	DirectXCommon *dxCommon_ = nullptr;

	SrvManager *srvManager_ = nullptr;

	// SRVインデックスの開始番号
	static uint32_t kSRVIndexTop;
};