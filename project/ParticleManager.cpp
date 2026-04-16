#include "ParticleManager.h"
#include "TextureManager.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "WinApp.h"
#include <cassert>

using namespace math;

ParticleManager *ParticleManager::GetInstance() {
    static ParticleManager instance;
    return &instance;
}

void ParticleManager::Initialize(DirectXCommon *dxCommon, SrvManager *srvManager) {
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

    accelerationField.acceleration = { 15.0f,0.0f,0.0f };
    accelerationField.area.min = { -1.0f,-1.0f,-1.0f };
    accelerationField.area.max = { 1.0f,1.0f,1.0f };

    cameraTransform.translate = { 0.0f, 23.0f, 10.0f };
    cameraTransform.rotate = { 0.5f, 3.14f, 0.0f };
    cameraTransform.scale = { 1.0f,1.0f,1.0f };
}

void ParticleManager::Update() {

    const float kDeltaTime = 1.0f / 60.0f;

    // カメラ行列作成
    Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);

    Matrix4x4 viewMatrix = Inverse(cameraMatrix);

    Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClientHeight), 0.1f, 100.0f);

    // ビルボード行列
    Matrix4x4 billboardMatrix = MakeIdentity4x4();

    if (isBillboard) {
        billboardMatrix = Inverse(viewMatrix);

        // 平行移動削除
        billboardMatrix.m[3][0] = 0.0f;
        billboardMatrix.m[3][1] = 0.0f;
        billboardMatrix.m[3][2] = 0.0f;
    }

    for (auto& [name, particleGroup] : particleGroups)
    {
        particleGroup.numInstance = 0;

        for (auto particleIterator = particleGroup.particles.begin();
            particleIterator != particleGroup.particles.end();)
        {
            Particle& particle = *particleIterator;

            // 寿命チェック
            if (particle.lifeTime <= particle.currentTime)
            {
                particleIterator = particleGroup.particles.erase(particleIterator);
                continue;
            }

            // 加速度
            if (IsCollision(accelerationField.area, particle.transform.translate))
            {
                particle.velocity += accelerationField.acceleration * kDeltaTime;
            }

            // 移動
            particle.transform.translate += particle.velocity * kDeltaTime;
            particle.currentTime += kDeltaTime;

            // 最大数チェック
            if (particleGroup.numInstance >= kNumMaxInstance)
            {
                ++particleIterator;
                continue;
            }

            // ワールド行列
            Matrix4x4 worldMatrix;

            if (isBillboard)
            {
                Matrix4x4 scaleMatrix = MakeScaleMatrix(particle.transform.scale);
                Matrix4x4 translateMatrix = MakeTranslateMatrix(particle.transform.translate);

                worldMatrix = Multiply(scaleMatrix, billboardMatrix);
                worldMatrix = Multiply(worldMatrix, translateMatrix);
            } else
            {
                worldMatrix = MakeAffineMatrix(
                    particle.transform.scale,
                    particle.transform.rotate,
                    particle.transform.translate
                );
            }

            Matrix4x4 worldViewMatrix = Multiply(worldMatrix, viewMatrix);
            Matrix4x4 wvp = Multiply(worldViewMatrix, projectionMatrix);

            // GPU書き込み
            particleGroup.instancingData[particleGroup.numInstance].World = worldMatrix;
            particleGroup.instancingData[particleGroup.numInstance].WVP = wvp;
            particleGroup.instancingData[particleGroup.numInstance].color = particle.color;

            particleGroup.numInstance++;

            ++particleIterator;
        }
    }
}

void ParticleManager::Draw() {

    // ルートシグネチャを設定
    dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature.Get());

    // PSOを設定
    dxCommon_->GetCommandList()->SetPipelineState(graphicsPipelineState.Get());

    // プリミティブトポロジーを設定
    dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // VBVを設定
    dxCommon_->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);

    for (auto &[name, particleGroup] : particleGroups) {

        if (particleGroup.numInstance == 0) {
            continue;
        }

        // テクスチャSRV
        dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(particleGroup.material.textureIndex));

        // インスタンシングSRV
        dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(3, srvManager_->GetGPUDescriptorHandle(particleGroup.instancingSrvIndex));

        // Draw
        dxCommon_->GetCommandList()->DrawInstanced(UINT(modelData_.vertices.size()), particleGroup.numInstance, 0, 0);
    }
}

void ParticleManager::CreateParticleGroup(const std::string name, const std::string textureFilePath)
{
    // 同じ名前のグループがないか確認
    assert(particleGroups.find(name) == particleGroups.end());

    // グループ作成
    ParticleGroup &newGroup = particleGroups[name];

    // テクスチャ読み込み
    TextureManager::GetInstance()->LoadTexture(textureFilePath);

    // マテリアル情報設定
    newGroup.material.textureFilePath = textureFilePath;
    newGroup.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);

    // インスタンス数初期化
    newGroup.numInstance = 0;

    // インスタンシング用バッファ作成
    newGroup.instancingResource = dxCommon_->CreateBufferResource(sizeof(ParticleForGPU) * kNumMaxInstance);

    // MapしてCPUから書き込み可能にする
    newGroup.instancingResource->Map(0, nullptr, reinterpret_cast<void **>(&newGroup.instancingData));

    // SRVインデックス取得
    newGroup.instancingSrvIndex = srvManager_->Allocate();

    // StructuredBuffer用SRV作成
    srvManager_->CreateSRVforStructuredBuffer(newGroup.instancingSrvIndex, newGroup.instancingResource.Get(), kNumMaxInstance, sizeof(ParticleForGPU));
}

void ParticleManager::Emit(const std::string name, const math::Vector3 &position, uint32_t count)
{
    assert(particleGroups.find(name) != particleGroups.end());

    ParticleGroup& group = particleGroups[name];

    std::uniform_real_distribution<float> posRand(-0.5f, 0.5f);
    std::uniform_real_distribution<float> velRand(-1.0f, 1.0f);
    std::uniform_real_distribution<float> colorRand(0.0f, 1.0f);
    std::uniform_real_distribution<float> lifeRand(0.8f, 1.5f);

    for (uint32_t i = 0; i < count; i++)
    {
        Particle particle {};

        particle.transform.translate =
        {
            position.x + posRand(randomEngine),
            position.y + posRand(randomEngine),
            position.z + posRand(randomEngine)
        };

        particle.transform.scale = { 0.5f,0.5f,0.5f };

        particle.velocity =
        {
            velRand(randomEngine),
            velRand(randomEngine),
            velRand(randomEngine)
        };

        particle.color =
        {
            colorRand(randomEngine),
            colorRand(randomEngine),
            colorRand(randomEngine),
            1.0f
        };

        particle.lifeTime = lifeRand(randomEngine);
        particle.currentTime = 0.0f;

        group.particles.push_back(particle);
    }
}

bool ParticleManager::IsCollision(const math::AABB &aabb, const math::Vector3 &point)
{
    if (point.x < aabb.min.x || point.x > aabb.max.x) return false;
    if (point.y < aabb.min.y || point.y > aabb.max.y) return false;
    if (point.z < aabb.min.z || point.z > aabb.max.z) return false;

    return true;
}