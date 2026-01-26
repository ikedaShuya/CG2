#pragma once
#include <string>
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"

class DirectXCommon;

// テクスチャマネージャー
class TextureManager
{
public:
	// シングルトンインスタンスの取得
	static TextureManager *GetInstance();
	// 終了
	void Finalize();

	// 初期化
	void Initialize();

	/// <summary>
	/// テクスチャファイルの読み込み
	/// </summary>
	/// <param name="filePath">テクスチャファイルのパス</param>
	void LoadTexture(const std::string &filePath);

	void ReleaseIntermediateResources();

	// SRVインデックスの開始番号
	uint32_t GetTextureIndexByFilePath(const std::string &filePath);

	// テクスチャ番号からGPUハンドルを取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(uint32_t textureIndex);

	void SetDirectXCommon(DirectXCommon *dxCommon);

private:
	static TextureManager *instance;

	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(TextureManager &) = delete;
	TextureManager &operator=(TextureManager &) = delete;

private:
	// テクスチャ1枚分のデータ
	struct TextureData {
		std::string filePath;
		DirectX::TexMetadata metadata;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
	};

	// テクスチャデータ
	std::vector<TextureData> textureDatas;

	DirectXCommon *dxCommon_ = nullptr;

	// SRVインデックスの開始番号
	static uint32_t kSRVIndexTop;
};