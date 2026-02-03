#pragma once
#include <wrl.h>
#include <d3d12.h>

class DirectXCommon;
class Camera;

// 3Dオブジェクト共通部
class Object3dCommon 
{
public: // メンバ関数
	// 初期化
	void Initialize(DirectXCommon *dxCommon);

	DirectXCommon *GetDxCommon() const { return dxCommon_; }

	void SetCommonRenderSetting();

	// setter
	void SetDefaultCamera(Camera *camera) { this->defaultCamera = camera; }
	// getter
	Camera *GetDefaultCamera() const { return defaultCamera; }

private:
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState = nullptr;

	// ルードシグネチャの作成
	void CreateRootSignature();
	// グラフィックスパイプラインの生成
	void CreateGraphicsPipelineState();

	DirectXCommon *dxCommon_ = nullptr;

	Camera *defaultCamera = nullptr;
};