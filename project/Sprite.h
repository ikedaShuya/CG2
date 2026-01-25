#pragma once
#include <cstdint>
#include <wrl.h>
#include <d3d12.h>
#include "MathFunctions.h"

class SpriteCommon;

// スプライト
class Sprite
{
public:
	// 頂点データ
	struct VertexData
	{
		math::Vector4 position;
		math::Vector2 texcoord;
		math::Vector3 normal;
	};

	// マテリアルデータ
	struct Material
	{
		math::Vector4 color;
		int32_t enableLighting;
		float padding[3];
		math::Matrix4x4 uvTransform;
	};

	// 座標変換行列データ
	struct TransformationMatrix
	{
		math::Matrix4x4 WVP;
		math::Matrix4x4 World;
	};

public: // メンバ関数
	// 初期化
	void Initialize(SpriteCommon *spriteCommon);
	// 更新
	void Update();
	// 描画
	void Draw();

	// getter
	const math::Vector2 &GetPosition() { return position_; }
	// setter
	void SetPosition(const math::Vector2 &position) { this->position_ = position; }

	float GetRotation() const { return rotation_; }
	void SetRotation(float rotation) { this->rotation_ = rotation; }

	const math::Vector4 &GetColor() const { return materialData->color; }
	void SetColor(const math::Vector4 &color) { materialData->color = color; }

	const math::Vector2 &GetSize() const { return size_; }
	void SetSize(const math::Vector2 &size) { this->size_ = size; }

private:
	SpriteCommon *spriteCommon = nullptr;

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
	// バッファリソース内のデータを指すポインタ
	VertexData *vertexData = nullptr;
	uint32_t *indexData = nullptr;
	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	void InitializeBuffers();

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	// バッファリソース内のデータを指すポインタ
	Material *materialData = nullptr;

	void InitializeMaterial();

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
	// バッファリソース内のデータを指すポインタ
	TransformationMatrix *transformationMatrixData = nullptr;
	void InitializeTransformationMatrixResource();

	math::Transform transform;

	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU {};
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU {};

	math::Vector2 position_ = { 0.0f,0.0f };
	float rotation_ = 0.0f;
	math::Vector2 size_ = { 64.0f,64.0f };
};