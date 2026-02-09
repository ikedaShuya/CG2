#pragma once
#include <wrl.h>
#include <d3d12.h>

class DirectXCommon;

// スプライト共通部
class SpriteCommon
{
public: // メンバ関数
	// 初期化
	void Initialize(DirectXCommon *dxCommon);

	DirectXCommon *GetDxCommon() const { return dxCommon_; }

	// 共通描画設定
	void SetupCommonDrawing();

private:
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState = nullptr;

	// ルードシグネチャの作成
	void CreateRootSignature();
	// グラフィックスパイプラインの生成
	void CreateGraphicsPipelineState();

	DirectXCommon *dxCommon_;
};