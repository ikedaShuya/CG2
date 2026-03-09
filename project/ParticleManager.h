#pragma once
#include <string>
#include <list>
#include <wrl.h>
#include <d3d12.h>
#include <unordered_map>
#include "MathFunctions.h"

class DirectXCommon;
class SrvManager;

// パーティクルマネージャー
class ParticleManager
{
public:

	// シングルトン取得
	static ParticleManager *GetInstance();

	// 初期化
	void Initialize(DirectXCommon *dxCommon, SrvManager *srvManager);

	// 更新
	void Update();

	// 描画
	void Draw();

	void CreateParticleGroup(const std::string name, const std::string textureFilePath);

	void Emit(const std::string name, const math::Vector3 &position, uint32_t count);

private:

	ParticleManager() = default;
	~ParticleManager() = default;
	ParticleManager(ParticleManager &) = delete;
	ParticleManager &operator=(ParticleManager &) = delete;

private:

	// DirectX関連
	DirectXCommon *dxCommon_ = nullptr;
	SrvManager *srvManager_ = nullptr;

	// 頂点データ
	struct VertexData
	{
		math::Vector4 position;
		math::Vector2 texcoord;
	};

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

	struct Particle
	{
		math::Transform transform;
		math::Vector3 velocity;
		math::Vector4 color;
		float lifeTime;
		float currentTime;
	};

	struct ParticleForGPU
	{
		math::Matrix4x4 WVP;
		math::Matrix4x4 World;
		math::Vector4 color;
	};

	struct ParticleGroup
	{
		MaterialData material;
		std::list<Particle> particles;
		uint32_t instancingSrvIndex;
		Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource;
		uint32_t numInstance = 0;
		ParticleForGPU *instancingData = nullptr;
	};

	struct AccelerationField
	{
		math::Vector3 acceleration; //!< 加速度
		math::AABB area; //!<　範囲
	};

	std::unordered_map<std::string, ParticleGroup> particleGroups;

	bool IsCollision(const math::AABB &aabb, const math::Vector3 &point);

	bool isBillboard = true;

	// カメラのTransform
	math::Transform cameraTransform;

	AccelerationField accelerationField;

	// 最大インスタンス数
	static const uint32_t kNumMaxInstance = 1000;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState = nullptr;

	// 頂点バッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView {};

	ModelData modelData_;
	D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU;
};