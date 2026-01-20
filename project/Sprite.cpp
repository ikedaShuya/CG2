#include "Sprite.h"
#include "SpriteCommon.h"
#include "DirectXCommon.h"

using namespace math;

void Sprite::Initialize(SpriteCommon *spriteCommon)
{
	// 引数で受け取ってメンバ変数に記録する
	this->spriteCommon = spriteCommon;

	InitializeBuffers();
	InitializeMaterial();
	InitializeTransformationMatrixResource();

	// Textureを読んで転送する
	DirectX::ScratchImage mipImages = spriteCommon->GetDxCommon()->LoadTexture("resources/textures/uvChecker.png");
	const DirectX::TexMetadata &metadata = mipImages.GetMetadata();
	textureResource = spriteCommon->GetDxCommon()->CreateTextureResource(metadata);
	intermediateResource = spriteCommon->GetDxCommon()->UploadTextureData(textureResource, mipImages);

	// metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	// SRVを作成するDescriptorHeapの場所を決める
	textureSrvHandleCPU = spriteCommon->GetDxCommon()->GetSRVCPUDescriptorHandle(1);
	textureSrvHandleGPU = spriteCommon->GetDxCommon()->GetSRVGPUDescriptorHandle(1);
	// SRVの生成
	spriteCommon->GetDxCommon()->GetDevice()->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);
}

void Sprite::Update()
{
	transform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	Matrix4x4 viewMatrix = MakeIdentity4x4();
	Matrix4x4 projectionMatrix = MakeOrthographicMatrix(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0.0f, 100.0f);
	transformationMatrixData->WVP = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
	transformationMatrixData->World = worldMatrix;
}

void Sprite::Draw()
{
	spriteCommon->GetDxCommon()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView); // VBVを設定
	spriteCommon->GetDxCommon()->GetCommandList()->IASetIndexBuffer(&indexBufferView); // IBVを設定

	spriteCommon->GetDxCommon()->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	spriteCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
	// TransformationMatrixCBufferの場所を設定
	spriteCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResource->GetGPUVirtualAddress());

	spriteCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
	//描画!(DrawCall/ドローコール)6個のインデックスを使用し1つのインスタンスを描画。その他は当面0で良い
	spriteCommon->GetDxCommon()->GetCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void Sprite::InitializeBuffers()
{
	// 頂点・インデックス用リソースを作成
	vertexResource = spriteCommon->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * 4);
	indexResource = spriteCommon->GetDxCommon()->CreateBufferResource(sizeof(uint32_t) * 6);

	// 頂点バッファビューの設定
	// リソースの先頭のアドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点6つ分のサイズ
	vertexBufferView.SizeInBytes = sizeof(VertexData) * 4;
	// 1頂点あたりのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	// インデックスバッファビューの設定
	// リソースの先頭のアドレスから使う
	indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
	// 使用するリソースのサイズはインデックス6つ分のサイズ
	indexBufferView.SizeInBytes = sizeof(uint32_t) * 6;
	// インデックスはuint32_tとする
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	// 頂点データをマップして書き込み
	vertexResource->Map(0, nullptr, reinterpret_cast<void **>(&vertexData));
	vertexData[0].position = { 0.0f,360.0f,0.0f,1.0f };
	vertexData[0].texcoord = { 0.0f,1.0f };
	vertexData[0].normal = { 0.0f,0.0f,-1.0f };

	vertexData[1].position = { 0.0f,0.0f,0.0f,1.0f };
	vertexData[1].texcoord = { 0.0f,0.0f };
	vertexData[1].normal = { 0.0f,0.0f,-1.0f };

	vertexData[2].position = { 640.0f,360.0f,0.0f,1.0f };
	vertexData[2].texcoord = { 1.0f,1.0f };
	vertexData[2].normal = { 0.0f,0.0f,-1.0f };

	vertexData[3].position = { 640.0f,0.0f,0.0f,1.0f };
	vertexData[3].texcoord = { 1.0f,0.0f };
	vertexData[3].normal = { 0.0f,0.0f,-1.0f };

	// インデックスデータをマップして書き込み
	indexResource->Map(0, nullptr, reinterpret_cast<void **>(&indexData));
	indexData[0] = 0;
	indexData[1] = 1;
	indexData[2] = 2;
	indexData[3] = 1;
	indexData[4] = 3;
	indexData[5] = 2;
}

void Sprite::InitializeMaterial()
{
	// マテリアルリソースを作成
	materialResource = spriteCommon->GetDxCommon()->CreateBufferResource(sizeof(Material));

	// バッファをCPUアクセス用にマップしてポインタ取得
	materialResource->Map(0, nullptr, reinterpret_cast<void **>(&materialData));

	// マテリアルデータの初期値を書き込む
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = false;
	materialData->uvTransform = MakeIdentity4x4();
}

void Sprite::InitializeTransformationMatrixResource()
{
	// 座標変換行列リソースを作成
	transformationMatrixResource = spriteCommon->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));

	// 書き込むためのアドレスを取得
	transformationMatrixResource->Map(0, nullptr, reinterpret_cast<void **>(&transformationMatrixData));
	// 単位行列を書きこんでおく
	transformationMatrixData->WVP = MakeIdentity4x4();
	transformationMatrixData->World = MakeIdentity4x4();
}