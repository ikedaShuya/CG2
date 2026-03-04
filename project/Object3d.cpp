#include "Object3d.h"
#include "Object3dCommon.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "Model.h"
#include "ModelManager.h"
#include <numbers>

using namespace math;

std::random_device seedGenerator;
std::mt19937 randomEngine(seedGenerator());

void Object3d::Initialize(Object3dCommon *object3dCommon)
{
	this->object3dCommon = object3dCommon;

	instancingSrvHandleGPU = object3dCommon->GetDxCommon()->GetSRVGPUDescriptorHandle(3);

	// ===== GPUリソース生成 =====
	CreateDirectionalLight();
	CreateInstancingBuffer();

	// ===== Transform初期化 =====
	transform = {
		{1.0f, 1.0f, 1.0f},   // scale
		{0.0f, 0.0f, 0.0f},   // rotate
		{0.0f, 0.0f, 0.0f}    // translate
	};

	cameraTransform = {
		{1.0f, 1.0f, 1.0f},   // scale
		{std::numbers::pi_v<float> / 3.0f, std::numbers::pi_v<float>, 0.0f},   // rotate
		{0.0f, 23.0f, 10.0f}  // translate
	};
}

void Object3d::Update()
{
	const float kDeltaTime = 1.0f / 60.0f;

	// ===== ビュー行列（カメラ） =====
	Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
	Matrix4x4 viewMatrix = Inverse(cameraMatrix);

	// ===== 射影行列 =====
	Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClientHeight), 0.1f, 100.0f);

	Matrix4x4 billboardMatrix = MakeIdentity4x4();

	if (isBillboard)
	{
		Matrix4x4 backToFrontMatrix = MakeRotateYMatrix(std::numbers::pi_v<float>);
		billboardMatrix = Multiply(backToFrontMatrix, cameraMatrix);
		billboardMatrix.m[3][0] = 0.0f;
		billboardMatrix.m[3][1] = 0.0f;
		billboardMatrix.m[3][2] = 0.0f;
	}

	numInstance = 0; // 描画すべきインスタンス数
	for (uint32_t index = 0; index < kNumMaxInstance; ++index) {

		if (particles[index].lifeTime <= particles[index].currentTime) { // 生存期間を過ぎていたら更新せず描画対象にしない
			particles[index] = MakeNewParticle(randomEngine);
		}

		particles[index].transform.translate += particles[index].velocity * kDeltaTime;
		particles[index].currentTime += kDeltaTime;

		float alpha = 1.0f - (particles[index].currentTime / particles[index].lifeTime);
		particles[index].color.w = alpha;

		Matrix4x4 scaleMatrix = MakeScaleMatrix(particles[index].transform.scale);
		Matrix4x4 translateMatrix = MakeTranslateMatrix(particles[index].transform.translate);

		Matrix4x4 worldMatrix = Multiply(Multiply(scaleMatrix, billboardMatrix), translateMatrix);
		Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));

		instancingData[numInstance].WVP = worldViewProjectionMatrix;
		instancingData[numInstance].World = worldMatrix;
		instancingData[numInstance].color = particles[index].color;
		++numInstance; // 生きているParticleの数を1つカウントする
	}
}

void Object3d::Draw()
{
	object3dCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(1, directionalLightResource->GetGPUVirtualAddress());
	object3dCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootDescriptorTable(3, instancingSrvHandleGPU);

	// 3Dモデルが割り当てられていれば描画する
	if (model) {
		model->Draw(numInstance);
	}
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

void Object3d::CreateInstancingBuffer()
{
	instancingResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(ParticleForGPU) * kNumMaxInstance);

	instancingResource->Map(0, nullptr, reinterpret_cast<void **>(&instancingData));

	for (uint32_t index = 0; index < kNumMaxInstance; ++index) {
		particles[index] = MakeNewParticle(randomEngine);
	}

	for (uint32_t index = 0; index < kNumMaxInstance; ++index) {
		instancingData[index].WVP = MakeIdentity4x4();
		instancingData[index].World = MakeIdentity4x4();
		instancingData[index].color = particles[index].color;
	}

	object3dCommon->CreateInstancingSRV(instancingResource.Get(), kNumMaxInstance, 3);
}

void Object3d::SetModel(const std::string &filePath)
{
	// モデルを検索してセットする
	model = ModelManager::GetInstance()->FindModel(filePath);
}

Object3d::Particle Object3d::MakeNewParticle(std::mt19937 &randomEngine)
{
	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
	std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
	std::uniform_real_distribution<float> distTime(1.0f, 3.0f);
	Particle particle {};
	particle.transform.scale = { 1.0f,1.0f,1.0f };
	particle.transform.rotate = { 0.0f,0.0f,0.0f };
	particle.transform.translate = { distribution(randomEngine),distribution(randomEngine),distribution(randomEngine) };
	particle.velocity = { distribution(randomEngine),distribution(randomEngine),distribution(randomEngine) };
	particle.color = { distColor(randomEngine),distColor(randomEngine) ,distColor(randomEngine) ,1.0f };
	particle.lifeTime = distTime(randomEngine);
	particle.currentTime = 0;
	return particle;
}