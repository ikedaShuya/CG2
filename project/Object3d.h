#pragma once
#include <string>
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include "MathFunctions.h"

class Object3dCommon;
class DirectXCommon;
class Model;

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

	// ===== リソース生成 =====
	void CreateTransformationMatrixResource();
	void CreateDirectionalLight();

	// setter
	void SetModel(Model *model) { this->model = model; }

	// setter
	void SetScale(const math::Vector3 &scale) { transform.scale = scale; }
	void SetRotate(const math::Vector3 &rotate) { transform.rotate = rotate; }
	void SetTranslate(const math::Vector3 &translate) { transform.translate = translate; }

	// getter
	const math::Vector3 &GetScale() const { return transform.scale; }
	const math::Vector3 &GetRotate() const { return transform.rotate; }
	const math::Vector3 &GetTranslate() const { return transform.translate; }

	void SetModel(const std::string &filePath);

private:

	// ===== 共通オブジェクト =====
	Object3dCommon *object3dCommon = nullptr;


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

	Model *model = nullptr;
};