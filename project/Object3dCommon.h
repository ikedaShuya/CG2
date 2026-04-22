#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <cstdint>
#include "MathFunctions.h"

class DirectXCommon;
class SrvManager;
class Camera;

// 3Dオブジェクト共通部
class Object3dCommon
{
public: // メンバ関数
	// 初期化
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	DirectXCommon* GetDxCommon() const {
		return dxCommon_;
	}

	void SetCommonRenderSetting();

	// setter
	void SetDefaultCamera(Camera* camera) {
		this->defaultCamera = camera;
	}
	// getter
	Camera* GetDefaultCamera() const {
		return defaultCamera;
	}

	void CreateInstancingSRV(ID3D12Resource* instancingResource, uint32_t numInstance, uint32_t descriptorIndex);

	// 座標変換行列
	struct TransformationMatrix
	{
		math::Matrix4x4 WVP;
		math::Matrix4x4 World;
	};

	struct ParticleForGPU
	{
		math::Matrix4x4 WVP;
		math::Matrix4x4 World;
		math::Vector4 color;
	};

private:
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState = nullptr;

	// ルードシグネチャの作成
	void CreateRootSignature();
	// グラフィックスパイプラインの生成
	void CreateGraphicsPipelineState();

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	Camera* defaultCamera = nullptr;
};