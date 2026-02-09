#pragma once
#include <string>
#include <cstdint>
#include <vector>  
#include <wrl.h>
#include <d3d12.h>
#include "MathFunctions.h"

class ModelCommon;

// 3Dモデル
class Model
{
public:

	// 頂点データ
	struct VertexData
	{
		math::Vector4 position;
		math::Vector2 texcoord;
		math::Vector3 normal;
	};

	// マテリアル読み込み用
	struct MaterialData
	{
		std::string textureFilePath;
		uint32_t textureIndex = 0;
	};

	// モデルデータ
	struct ModelData
	{
		std::vector<VertexData> vertices;
		MaterialData material;
	};

	// GPU用マテリアル
	struct Material
	{
		math::Vector4 color;
		int32_t enableLighting;
		float padding[3];
		math::Matrix4x4 uvTransform;
	};

public: // メンバ関数
	// 初期化
	void Initialize(ModelCommon *modelCommon, const std::string &directorypath, const std::string &filename);

	// 描画
	void Draw();

	// ===== モデル読み込み =====
	static MaterialData LoadMaterialTemplateFile(const std::string &directoryPath, const std::string &filename);

	static ModelData LoadObjFile(const std::string &directoryPath, const std::string &filename);

private:

	void CreateVertexBuffer(const std::vector<VertexData> &vertices);
	void CreateMaterialResource();

private:

	ModelCommon *modelCommon_;

	// ===== モデルデータ =====
	// objファイルから読み込んだモデル情報
	ModelData modelData_;

	// ===== 頂点バッファ =====
	// 頂点バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = nullptr;
	// バッファ内の頂点データを書き込むポインタ
	VertexData *vertexData = nullptr;
	// 頂点バッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView {};


	// ===== マテリアル =====
	// マテリアル用バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = nullptr;
	// マテリアルデータ書き込み用ポインタ
	Material *materialData = nullptr;
};