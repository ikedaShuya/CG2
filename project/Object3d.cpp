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

	emitter.transform.translate = { 0.0f,0.0f,0.0f };
	emitter.transform.rotate = { 0.0f,0.0f,0.0f };
	emitter.transform.scale = { 1.0f,1.0f,1.0f };
	emitter.count = 40;
	emitter.frequency = 0.5f; // 0.5秒ごとに発生
	emitter.frequencyTime = 0.0f; // 発生頻度用の時刻、0で初期化

	accelerationField.acceleration = { 15.0f,0.0f,0.0f };
	accelerationField.area.min = { -1.0f,-1.0f,-1.0f };
	accelerationField.area.max = { 1.0f,1.0f,1.0f };
}

void Object3d::Update()
{
	const float kDeltaTime = 1.0f / 60.0f;

	numInstance = 0;

	// ===== パーティクル発生 =====
	emitter.frequencyTime += kDeltaTime;
	if (emitter.frequencyTime >= emitter.frequency) {
		particles.splice(particles.end(), Emit(emitter, randomEngine));
		emitter.frequencyTime -= emitter.frequency;
	}

	// ===== カメラ行列 =====
	Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);

	Matrix4x4 viewMatrix = Inverse(cameraMatrix);

	// ===== 射影行列 =====
	Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClientHeight), 0.1f, 100.0f);

	// ===== ビルボード行列 =====
	Matrix4x4 billboardMatrix = MakeIdentity4x4();

	if (isBillboard) {
		billboardMatrix = Inverse(viewMatrix);

		// 平行移動を消す
		billboardMatrix.m[3][0] = 0.0f;
		billboardMatrix.m[3][1] = 0.0f;
		billboardMatrix.m[3][2] = 0.0f;
	}

	// ===== パーティクル更新 =====
	for (std::list<Particle>::iterator particleIterator = particles.begin(); particleIterator != particles.end();) {
		// 寿命チェック
		if ((*particleIterator).lifeTime <= (*particleIterator).currentTime) {
			particleIterator = particles.erase(particleIterator); // 生存期間過ぎたParticleはlistから消す。戻り値が次のイテレータとなる
			continue;
		}

		Matrix4x4 worldMatrix;

		// ===== ビルボード =====
		if (isBillboard) {
			Matrix4x4 scaleMatrix = MakeScaleMatrix(particleIterator->transform.scale);

			Matrix4x4 translateMatrix = MakeTranslateMatrix(particleIterator->transform.translate);

			worldMatrix = Multiply(scaleMatrix, billboardMatrix);
			worldMatrix = Multiply(worldMatrix, translateMatrix);

		} else {
			worldMatrix = MakeAffineMatrix(particleIterator->transform.scale, particleIterator->transform.rotate, particleIterator->transform.translate);
		}

		// ===== WVP計算 =====
		Matrix4x4 worldViewMatrix = Multiply(worldMatrix, viewMatrix);
		Matrix4x4 worldViewProjectionMatrix = Multiply(Multiply(worldMatrix, viewMatrix), projectionMatrix);

		// ===== インスタンシングデータ書き込み =====
		if (numInstance >= kNumMaxInstance) {
			break;
		}

		instancingData[numInstance].World = worldMatrix;
		instancingData[numInstance].WVP = worldViewProjectionMatrix;
		instancingData[numInstance].color = particleIterator->color;

		++numInstance;

		// ===== 位置更新 =====
		// Fieldの範囲内のParticleには加速度を適用する
		if (IsCollision(accelerationField.area, (*particleIterator).transform.translate)) {
			(*particleIterator).velocity += accelerationField.acceleration * kDeltaTime;
		}

		particleIterator->transform.translate += particleIterator->velocity * kDeltaTime;

		particleIterator->currentTime += kDeltaTime;

		++particleIterator;
	}
}

void Object3d::Draw()
{
	object3dCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(1, directionalLightResource->GetGPUVirtualAddress());
	object3dCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootDescriptorTable(3, instancingSrvHandleGPU);

	// 3Dモデルが割り当てられていれば描画する
	if (model && numInstance > 0) {
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

	object3dCommon->CreateInstancingSRV(instancingResource.Get(), kNumMaxInstance, 3);
}

void Object3d::SetModel(const std::string &filePath)
{
	// モデルを検索してセットする
	model = ModelManager::GetInstance()->FindModel(filePath);
}

Object3d::Particle Object3d::MakeNewParticle(std::mt19937 &randomEngine, const math::Vector3 &translate)
{
	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
	std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
	std::uniform_real_distribution<float> distTime(1.0f, 1.5f);
	Particle particle {};
	particle.transform.scale = { 1.0f,1.0f,1.0f };
	particle.transform.rotate = { 0.0f,0.0f,0.0f };
	particle.transform.translate = { emitter.transform.translate.x + distribution(randomEngine),emitter.transform.translate.y + distribution(randomEngine),emitter.transform.translate.z + distribution(randomEngine) };
	particle.velocity = { distribution(randomEngine),distribution(randomEngine),distribution(randomEngine) };
	particle.color = { distColor(randomEngine),distColor(randomEngine) ,distColor(randomEngine) ,1.0f };
	particle.lifeTime = distTime(randomEngine);
	particle.currentTime = 0;
	Vector3 randomTranslate { distribution(randomEngine),distribution(randomEngine),distribution(randomEngine) };
	particle.transform.translate = translate + randomTranslate;
	return particle;
}

std::list<Object3d::Particle> Object3d::Emit(const Emitter &emitter, std::mt19937 &randomEngine)
{
	std::list<Particle> particles;
	for (uint32_t count = 0; count < emitter.count; ++count) {
		particles.push_back(MakeNewParticle(randomEngine, emitter.transform.translate));
	}
	return particles;
}

bool Object3d::IsCollision(const AABB &aabb, const Vector3 &point)
{
	if (point.x < aabb.min.x || point.x > aabb.max.x) return false;
	if (point.y < aabb.min.y || point.y > aabb.max.y) return false;
	if (point.z < aabb.min.z || point.z > aabb.max.z) return false;

	return true;
}