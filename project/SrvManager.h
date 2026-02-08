#pragma once
#include <cstdint>
#include <wrl.h>
#include <d3d12.h>

class DirectXCommon;

// SRV管理
class SrvManager
{
public:

	// 初期化
	void Initialize(DirectXCommon *dxCommon);

	uint32_t Allocate();

	// 最大SRV数（最大テクスチャ枚数)
	static const uint32_t kMaxSRVCount;

	void PreDraw();

	void SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex);

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

private:
	DirectXCommon *directXCommon = nullptr;

	// SRV用のデスクリプタサイズ
	uint32_t descriptorSize;
	//SRV用デスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	// 次に使用するSRVインデックス
	uint32_t useIndex = 0;

	// SRV生成(テクスチャ用)
	void CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource *pResource, DXGI_FORMAT Format, UINT MipLevels);
	// SRV#nt (Structured Buffer#)
	void CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource *pResource, UINT numElements, UINT structureByteStride);
};