#pragma once
#include <string>
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include "MathFunctions.h"

class SkyboxCommon;
class DirectXCommon;

class Skybox
{
public:

	struct VertexData
	{
		math::Vector4 position;
		math::Vector2 texcoord;
		math::Vector3 normal;
	};

	struct TransformationMatrix
	{
		math::Matrix4x4 WVP;
		math::Matrix4x4 World;
	};

public:

	void Initialize(SkyboxCommon* skyboxCommon);

	void Update();

	void Draw();

	void CreateTransformationMatrixResource();

	// Cubemap設定
	void SetTexture(const std::string& filePath);

	void CreateVertexBuffer();
	void CreateIndexBuffer();

	// カメラ回転
	math::Vector3 GetCameraRotate() const;
	void SetCameraRotate(const math::Vector3& rotate);

	// カメラ位置
	math::Vector3 GetCameraTranslate() const;
	void SetCameraTranslate(const math::Vector3& translate);
	
private:

	SkyboxCommon* skyboxCommon = nullptr;

	// ===== 変換行列 =====
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource = nullptr;
	TransformationMatrix* transformationMatrixData = nullptr;

	// ===== Transform =====
	math::Transform transform;
	math::Transform cameraTransform;

	// ===== Cubemap =====
	std::string textureFilePath;
	uint32_t textureIndex = 0;

	// VertexBuffer
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferResource = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

	// IndexBuffer
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferResource = nullptr;
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};

	// Index数
	uint32_t indexCount = 0;
};