#pragma once
#include <string>
#include <vector>
#include <list>
#include <random>
#include <wrl.h>
#include <d3d12.h>
#include "MathFunctions.h"

class Object3dCommon;
class DirectXCommon;
class Model;
class Camera;

// 3Dオブジェクト
class Object3d
{
public:

	// ===== 既存 =====

	struct VertexData
	{
		math::Vector4 position;
		math::Vector2 texcoord;
		math::Vector3 normal;
	};

	struct MaterialData
	{
		std::string textureFilePath;
		uint32_t textureIndex = 0;
	};

	struct ModelData
	{
		std::vector<VertexData> vertices;
		MaterialData material;
	};

	struct Material
	{
		math::Vector4 color;
		int32_t enableLighting;
		float padding[3];
		math::Matrix4x4 uvTransform;
	};

	struct TransformationMatrix
	{
		math::Matrix4x4 WVP;
		math::Matrix4x4 World;
	};

	struct DirectionalLight
	{
		math::Vector4 color;
		math::Vector3 direction;
		float intensity;
	};

public:

	// ===== 基本処理 =====
	void Initialize(Object3dCommon* object3dCommon);
	void Update();
	void Draw();

	// ===== リソース =====
	void CreateTransformationMatrixResource(); // ← 元のやつ残す
	void CreateDirectionalLight();

	// ===== setter =====
	void SetModel(Model* model) {
		this->model = model;
	}
	void SetModel(const std::string& filePath);

	void SetScale(const math::Vector3& scale) {
		transform.scale = scale;
	}
	void SetRotate(const math::Vector3& rotate) {
		transform.rotate = rotate;
	}
	void SetTranslate(const math::Vector3& translate) {
		transform.translate = translate;
	}

	void SetCamera(Camera* camera) {
		this->camera = camera;
	}

	void SetCameraScale(const math::Vector3& scale) {
		cameraTransform.scale = scale;
	}
	void SetCameraRotate(const math::Vector3& rotate) {
		cameraTransform.rotate = rotate;
	}
	void SetCameraTranslate(const math::Vector3& translate) {
		cameraTransform.translate = translate;
	}

	// Light
	void SetLightDirection(const math::Vector3& dir) {
		directionalLightData->direction = dir;
	}
	void SetLightIntensity(float intensity) {
		directionalLightData->intensity = intensity;
	}
	void SetLightColor(const math::Vector4& color) {
		directionalLightData->color = color;
	}

	// ===== getter =====
	const math::Vector3& GetScale() const {
		return transform.scale;
	}
	const math::Vector3& GetRotate() const {
		return transform.rotate;
	}
	const math::Vector3& GetTranslate() const {
		return transform.translate;
	}

	const math::Vector3& GetCameraScale() const {
		return cameraTransform.scale;
	}
	const math::Vector3& GetCameraRotate() const {
		return cameraTransform.rotate;
	}
	const math::Vector3& GetCameraTranslate() const {
		return cameraTransform.translate;
	}

	const math::Vector3& GetLightDirection() const {
		return directionalLightData->direction;
	}
	float GetLightIntensity() const {
		return directionalLightData->intensity;
	}
	const math::Vector4& GetLightColor() const {
		return directionalLightData->color;
	}

private:

	// ===== 共通 =====
	Object3dCommon* object3dCommon = nullptr;

	// ===== 行列 =====
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource = nullptr;
	TransformationMatrix* transformationMatrixData = nullptr;

	// ===== ライト =====
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource = nullptr;
	DirectionalLight* directionalLightData = nullptr;

	// ===== Transform =====
	math::Transform transform;
	math::Transform cameraTransform;

	Model* model = nullptr;
	Camera* camera = nullptr;
};