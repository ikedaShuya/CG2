#include "Object3d.h"
#include "Object3dCommon.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "Model.h"
#include "ModelManager.h"
#include "Camera.h"

using namespace math;

void Object3d::Initialize(Object3dCommon *object3dCommon)
{
	this->object3dCommon = object3dCommon;
	this->camera = object3dCommon->GetDefaultCamera();

	// ===== GPUリソース生成 =====
	CreateTransformationMatrixResource();
	CreateDirectionalLight();

	// ===== Transform初期化 =====
	transform = {
		{1.0f, 1.0f, 1.0f},   // scale
		{0.0f, 0.0f, 0.0f},   // rotate
		{0.0f, 0.0f, 0.0f}    // translate
	};

	cameraTransform = {
		{1.0f, 1.0f, 1.0f},   // scale
		{0.3f, 0.0f, 0.0f},   // rotate
		{0.0f, 4.0f, -10.0f}  // translate
	};
}

void Object3d::Update()
{
	// ===== ワールド行列 =====
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);

	// ===== WVP行列 =====
	Matrix4x4 worldViewProjectionMatrix;
	if (camera) {
		const Matrix4x4 &viewProjectionMatrix = camera->GetViewProjectionMatrix();
		worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);
	} else {
		worldViewProjectionMatrix = worldMatrix;
	}

	// ===== 定数バッファへ反映 =====
	transformationMatrixData->World = worldMatrix;
	transformationMatrixData->WVP = worldViewProjectionMatrix;
}

void Object3d::Draw()
{
	object3dCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResource->GetGPUVirtualAddress());
	object3dCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());

	// 3Dモデルが割り当てられていれば描画する
	if (model) {
		model->Draw();
	}
}

void Object3d::CreateTransformationMatrixResource()
{
	// 座標変換行列用リソース作成
	transformationMatrixResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));

	// 書き込み用アドレス取得
	transformationMatrixResource->Map(0, nullptr, reinterpret_cast<void **>(&transformationMatrixData));

	// 単位行列で初期化
	transformationMatrixData->WVP = math::MakeIdentity4x4();
	transformationMatrixData->World = math::MakeIdentity4x4();
}

void Object3d::CreateDirectionalLight()
{
	// 平行光源用バッファ作成
	directionalLightResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(DirectionalLight));

	// 書き込みアドレス取得
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void **>(&directionalLightData));

	// 初期値設定
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->intensity = 1.0f;
}

void Object3d::SetModel(const std::string &filePath)
{
	// モデルを検索してセットする
	model = ModelManager::GetInstance()->FindModel(filePath);
}