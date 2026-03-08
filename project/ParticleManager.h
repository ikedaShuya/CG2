#pragma once
#include <string>
#include <list>
#include <wrl.h>
#include <d3d12.h>
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

private:

	ParticleManager() = default;
	~ParticleManager() = default;
	ParticleManager(ParticleManager &) = delete;
	ParticleManager &operator=(ParticleManager &) = delete;

private:

	// DirectX関連
	DirectXCommon *dxCommon_ = nullptr;
	SrvManager *srvManager_ = nullptr;

	struct MaterialData
	{
		std::string textureFilePath;
		uint32_t textureIndex = 0;
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
};