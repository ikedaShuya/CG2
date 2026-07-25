#include "SkyboxCommon.h"
#include "DirectXCommon.h"
#include <cassert>

void SkyboxCommon::Initialize(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;
	CreateGraphicsPipelineState();
}

void SkyboxCommon::SetCommonRenderSetting()
{
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature.Get());
	dxCommon_->GetCommandList()->SetPipelineState(graphicsPipelineState.Get());
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(
		D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void SkyboxCommon::CreateRootSignature()
{
	HRESULT hr;

	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// ===== DescriptorRange =====
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};

	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart =
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ===== RootParameter =====

	D3D12_ROOT_PARAMETER rootParameters[2] = {};

	// RootParameter[0]
	// VertexShader : TransformationMatrix b0
	rootParameters[0].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	// RootParameter[1]
	// PixelShader : TextureCube t0
	rootParameters[1].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].DescriptorTable.pDescriptorRanges =
		descriptorRange;
	rootParameters[1].DescriptorTable.NumDescriptorRanges =
		_countof(descriptorRange);

	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters =
		_countof(rootParameters);

	// ===== Sampler =====

	D3D12_STATIC_SAMPLER_DESC staticSampler{};

	staticSampler.Filter =
		D3D12_FILTER_MIN_MAG_MIP_LINEAR;

	staticSampler.AddressU =
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

	staticSampler.AddressV =
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

	staticSampler.AddressW =
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

	staticSampler.ComparisonFunc =
		D3D12_COMPARISON_FUNC_NEVER;

	staticSampler.MaxLOD =
		D3D12_FLOAT32_MAX;

	staticSampler.ShaderRegister = 0;

	staticSampler.ShaderVisibility =
		D3D12_SHADER_VISIBILITY_PIXEL;

	descriptionRootSignature.pStaticSamplers =
		&staticSampler;

	descriptionRootSignature.NumStaticSamplers = 1;

	// ===== Serialize =====

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

	hr = D3D12SerializeRootSignature(
		&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob,
		&errorBlob);

	if (FAILED(hr)) {
		assert(false);
	}

	hr = dxCommon_->GetDevice()->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature));

	assert(SUCCEEDED(hr));
}

void SkyboxCommon::CreateGraphicsPipelineState() {

	HRESULT hr;

	CreateRootSignature();

	//----InputLayoutの設定を行う----

	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	//----BlendStateの設定を行う----

	// BlendStateの設定
	D3D12_BLEND_DESC blendDesc{};
	// すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = FALSE;

	// 1. 通常アルファブレンド（透明感が出る）
	// ソースのアルファ値で透過させ、背景と自然に混ざる
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;        // ソースのアルファ
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;   // 1 - ソースのアルファ
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;            // 加算

	// 2. 加算ブレンド（色が明るくなる）
	// 重ねると光を足すような効果
	/*blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;*/

	// 3. 乗算ブレンド（色を暗くする）
	// 重ねると暗くなる（影や影響表現に便利）
	/*blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_DEST_COLOR;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;*/

	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

	//----RasterizerStateの設定を行う----

	// RasterzerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	// カリングしない（裏面も表示させる）
	rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	//----ShaderをCompileする----

	// Shaderをコンパイルする
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Skybox.VS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Skybox.PS.hlsl", L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	//----PSOを生成する----

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get(); // RootSignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc; // InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() }; // VertexShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() }; // PixelShader
	graphicsPipelineStateDesc.BlendState = blendDesc; // BlenState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc; // RasterizerState
	// 書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトポロジ（形状）のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定（気にしなくて良い）
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	//----DepthStencilStateの設定----

	// DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	// Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	// 書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	// 比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,
		IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));
}