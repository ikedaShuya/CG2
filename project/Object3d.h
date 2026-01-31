#pragma once
#include <string>
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include "MathFunctions.h"

class Object3dCommon;
class DirectXCommon;

// 3Dオブジェクト
class Object3d
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

	// 座標変換行列
	struct TransformationMatrix
	{
		math::Matrix4x4 WVP;
		math::Matrix4x4 World;
	};

	// 平行光源
	struct DirectionalLight
	{
		math::Vector4 color;      // ライトの色
		math::Vector3 direction;  // ライトの向き
		float intensity;          // 輝度
	};

public: // メンバ関数

	// 初期化
	void Initialize(Object3dCommon *object3dCommon);

	// 更新
	void Update();

	// 描画
	void Draw();

	// ===== モデル読み込み =====
	static MaterialData LoadMaterialTemplateFile(const std::string &directoryPath, const std::string &filename);

	static ModelData LoadObjFile(const std::string &directoryPath, const std::string &filename);

	// ===== リソース生成 =====
	void CreateVertexBuffer(const std::vector<VertexData> &vertices);
	void CreateMaterialResource();
	void CreateTransformationMatrixResource();
	void CreateDirectionalLight();

private:

	// ===== 共通オブジェクト =====
	Object3dCommon *object3dCommon = nullptr;


	// ===== モデルデータ =====
	// objファイルから読み込んだモデル情報
	ModelData modelData;

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


	// ===== 変換行列 =====
	// 変換行列用バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource = nullptr;
	// 変換行列データ書き込み用ポインタ
	TransformationMatrix *transformationMatrixData = nullptr;


	// ===== 平行光源 =====
	// 平行光源用バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource = nullptr;
	// 平行光源データ書き込み用ポインタ
	DirectionalLight *directionalLightData = nullptr;


	// ===== Transform =====
	// オブジェクトのTransform
	math::Transform transform;
	// カメラのTransform
	math::Transform cameraTransform;
};