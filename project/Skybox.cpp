#include "Skybox.h"
#include "SkyboxCommon.h"
#include "DirectXCommon.h"
#include "TextureManager.h"

using namespace math;

void Skybox::Initialize(SkyboxCommon* skyboxCommon)
{
	this->skyboxCommon = skyboxCommon;

	// ===== GPUリソース生成 =====
	CreateTransformationMatrixResource();
	CreateVertexBuffer();
	CreateIndexBuffer();

	// ===== Transform初期化 =====
	transform = {
		{100.0f, 100.0f, 100.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};

	cameraTransform = {
		{1.0f, 1.0f, 1.0f},
		{0.3f, 0.0f, 0.0f},
		{0.0f, 4.0f, -10.0f}
	};
}

void Skybox::Update()
{
	// ===== Skyboxをカメラ位置に追従させる =====
	// Skyboxは常にカメラの位置に置く
	transform.translate = cameraTransform.translate;

	// ===== ワールド行列 =====
	Matrix4x4 worldMatrix =
		MakeAffineMatrix(
			transform.scale,
			transform.rotate,
			transform.translate);

	// ===== ビュー行列 =====
	Matrix4x4 cameraMatrix =
		MakeAffineMatrix(
			cameraTransform.scale,
			cameraTransform.rotate,
			cameraTransform.translate);

	Matrix4x4 viewMatrix =
		Inverse(cameraMatrix);

	// ===== 射影行列 =====
	Matrix4x4 projectionMatrix =
		MakePerspectiveFovMatrix(
			0.45f,
			float(WinApp::kClientWidth) /
			float(WinApp::kClientHeight),
			0.1f,
			1000.0f);

	// ===== 定数バッファへ反映 =====
	transformationMatrixData->WVP =
		worldMatrix *
		viewMatrix *
		projectionMatrix;

	transformationMatrixData->World =
		worldMatrix;
}

void Skybox::Draw()
{
	// ===== Skybox用PSO・RootSignature設定 =====
	skyboxCommon->SetCommonRenderSetting();

	auto* commandList =
		skyboxCommon->GetDxCommon()->GetCommandList();

	// ===== RootParameter[0] =====
	// VertexShader : TransformationMatrix b0
	commandList->SetGraphicsRootConstantBufferView(
		0,
		transformationMatrixResource->GetGPUVirtualAddress());

	// ===== RootParameter[1] =====
	// PixelShader : TextureCube t0
	commandList->SetGraphicsRootDescriptorTable(
		1,
		TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex));

	// ===== VertexBuffer設定 =====
	commandList->IASetVertexBuffers(
		0,
		1,
		&vertexBufferView);

	// ===== IndexBuffer設定 =====
	commandList->IASetIndexBuffer(
		&indexBufferView);

	// ===== Skybox Cube描画 =====
	commandList->DrawIndexedInstanced(
		indexCount,
		1,
		0,
		0,
		0);
}

void Skybox::CreateTransformationMatrixResource()
{
	// ===== 座標変換行列用リソース作成 =====
	transformationMatrixResource =
		skyboxCommon->GetDxCommon()->CreateBufferResource(
			sizeof(TransformationMatrix));

	// ===== 書き込み用アドレス取得 =====
	transformationMatrixResource->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&transformationMatrixData));

	// ===== 単位行列で初期化 =====
	transformationMatrixData->WVP =
		math::MakeIdentity4x4();

	transformationMatrixData->World =
		math::MakeIdentity4x4();
}

void Skybox::CreateVertexBuffer()
{
	// ===== Skyboxの頂点データ =====
	VertexData vertices[] =
	{
		// 前面
		{{-1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
		{{-1.0f,  1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
		{{ 1.0f,  1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
		{{ 1.0f, -1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},

		// 背面
		{{ 1.0f, -1.0f,  1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
		{{ 1.0f,  1.0f,  1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
		{{-1.0f,  1.0f,  1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
		{{-1.0f, -1.0f,  1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},

		// 上面
		{{-1.0f, 1.0f, -1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
		{{-1.0f, 1.0f,  1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
		{{ 1.0f, 1.0f,  1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
		{{ 1.0f, 1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},

		// 下面
		{{-1.0f, -1.0f,  1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
		{{-1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
		{{ 1.0f, -1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
		{{ 1.0f, -1.0f,  1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},

		// 右面
		{{1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
		{{1.0f,  1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
		{{1.0f,  1.0f,  1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
		{{1.0f, -1.0f,  1.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},

		// 左面
		{{-1.0f, -1.0f,  1.0f, 1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
		{{-1.0f,  1.0f,  1.0f, 1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
		{{-1.0f,  1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
		{{-1.0f, -1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
	};

	// ===== VertexBuffer作成 =====
	const size_t vertexBufferSize =
		sizeof(vertices);

	vertexBufferResource =
		skyboxCommon->GetDxCommon()->CreateBufferResource(
			vertexBufferSize);

	// ===== VertexBufferへデータ転送 =====
	void* mappedData = nullptr;

	vertexBufferResource->Map(
		0,
		nullptr,
		&mappedData);

	memcpy(
		mappedData,
		vertices,
		vertexBufferSize);

	vertexBufferResource->Unmap(
		0,
		nullptr);

	// ===== VertexBufferView設定 =====
	vertexBufferView.BufferLocation =
		vertexBufferResource->GetGPUVirtualAddress();

	vertexBufferView.SizeInBytes =
		static_cast<UINT>(vertexBufferSize);

	vertexBufferView.StrideInBytes =
		sizeof(VertexData);
}

void Skybox::CreateIndexBuffer()
{
	// ===== Cubeのインデックス =====
	uint32_t indices[] =
	{
		// 前面
		0, 1, 2,
		0, 2, 3,

		// 背面
		4, 5, 6,
		4, 6, 7,

		// 上面
		8, 9, 10,
		8, 10, 11,

		// 下面
		12, 13, 14,
		12, 14, 15,

		// 右面
		16, 17, 18,
		16, 18, 19,

		// 左面
		20, 21, 22,
		20, 22, 23
	};

	indexCount =
		static_cast<uint32_t>(std::size(indices));

	// ===== IndexBuffer作成 =====
	const size_t indexBufferSize =
		sizeof(indices);

	indexBufferResource =
		skyboxCommon->GetDxCommon()->CreateBufferResource(
			indexBufferSize);

	// ===== IndexBufferへデータ転送 =====
	void* mappedData = nullptr;

	indexBufferResource->Map(
		0,
		nullptr,
		&mappedData);

	memcpy(
		mappedData,
		indices,
		indexBufferSize);

	indexBufferResource->Unmap(
		0,
		nullptr);

	// ===== IndexBufferView設定 =====
	indexBufferView.BufferLocation =
		indexBufferResource->GetGPUVirtualAddress();

	indexBufferView.SizeInBytes =
		static_cast<UINT>(indexBufferSize);

	indexBufferView.Format =
		DXGI_FORMAT_R32_UINT;
}

void Skybox::SetTexture(const std::string& filePath)
{
	// ===== Cubemap読み込み =====
	TextureManager::GetInstance()->LoadTexture(filePath);

	// ===== テクスチャ番号取得 =====
	textureIndex =
		TextureManager::GetInstance()->GetTextureIndexByFilePath(
			filePath);
}

math::Vector3 Skybox::GetCameraRotate() const
{
	return cameraTransform.rotate;
}

void Skybox::SetCameraRotate(const math::Vector3& rotate)
{
	cameraTransform.rotate = rotate;
}

math::Vector3 Skybox::GetCameraTranslate() const
{
	return cameraTransform.translate;
}

void Skybox::SetCameraTranslate(const math::Vector3& translate)
{
	cameraTransform.translate = translate;
}