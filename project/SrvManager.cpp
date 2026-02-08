#include "SrvManager.h"
#include "DirectXCommon.h"

const uint32_t SrvManager::kMaxSRVCount = 512;

void SrvManager::Initialize(DirectXCommon *dxCommon)
{
	// 引数で受け取ってメンバ変数に記録する
	this->directXCommon = dxCommon;

	// デスクリプタヒープの生成
	descriptorHeap = directXCommon->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSRVCount, true);
	//デスクリプタ1個分のサイズを取得して記録
	descriptorSize = directXCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	useIndex = 0;
}

uint32_t SrvManager::Allocate()
{
	// 上限に達していないかチェック
	assert(useIndex < kMaxSRVCount);

	// returnする場合を一旦記録しておく
	int index = useIndex;
	// 次回のために番号を1進める
	useIndex++;
	// 上で記録した番号をreturn
	return index;
}

void SrvManager::PreDraw() {

	// 描画用のDescriptorHeapの設定
	ID3D12DescriptorHeap *descriptorHeaps[] = { descriptorHeap.Get() };
	directXCommon->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);
}

void SrvManager::SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex)
{
	directXCommon->GetCommandList()->SetGraphicsRootDescriptorTable(RootParameterIndex, GetGPUDescriptorHandle(srvIndex));
}

D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}

void SrvManager::CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource *pResource, DXGI_FORMAT Format, UINT MipLevels)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
	// フォーマット（テクスチャの形式）
	srvDesc.Format = Format;

	// シェーダーからの見え方（RGBAなど）
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	// テクスチャ2Dとして使う
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

	// Texture2D 用の詳細設定
	srvDesc.Texture2D.MipLevels = MipLevels; // ミップマップ数

	directXCommon->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex));
}

void SrvManager::CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource *pResource, UINT numElements, UINT structureByteStride)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};

	// フォーマットなし（構造化バッファ専用）
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;

	// シェーダーからの見え方（お決まり）
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	// Structured Buffer として使う
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;

	// Buffer 用の詳細設定
	srvDesc.Buffer.FirstElement = 0;                 // 先頭要素
	srvDesc.Buffer.NumElements = numElements;        // 要素数
	srvDesc.Buffer.StructureByteStride = structureByteStride; // 1要素のサイズ
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	// SRV 作成
	directXCommon->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex));
}