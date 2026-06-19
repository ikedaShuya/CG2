#pragma once
#include <wrl.h>
#include <d3d12.h>

class DirectXCommon;

class SkyboxCommon
{
public:
	void Initialize(DirectXCommon* dxCommon);

	DirectXCommon* GetDxCommon() const { return dxCommon_; }

	void SetCommonRenderSetting();

private:
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState = nullptr;

	void CreateRootSignature();
	void CreateGraphicsPipelineState();

	DirectXCommon* dxCommon_ = nullptr;
};