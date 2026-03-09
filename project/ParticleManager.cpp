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

    for (auto &[name, particleGroup] : particleGroups) {

        // インスタンス数リセット
        particleGroup.numInstance = 0;

        for (std::list<Particle>::iterator particleIterator = particleGroup.particles.begin(); particleIterator != particleGroup.particles.end();) {

            // 寿命チェック
            if ((*particleIterator).lifeTime <= (*particleIterator).currentTime) {
                particleIterator = particleGroup.particles.erase(particleIterator); // 生存期間過ぎたParticleはlistから消す。戻り値が次のイテレータとなる
                continue;
            }

            // ===== 位置更新 =====
            // Fieldの範囲内のParticleには加速度を適用する
            if (IsCollision(accelerationField.area, (*particleIterator).transform.translate)) {
                (*particleIterator).velocity += accelerationField.acceleration * kDeltaTime;
            }

            particleIterator->transform.translate += particleIterator->velocity * kDeltaTime;

            particleIterator->currentTime += kDeltaTime;

            Matrix4x4 worldMatrix;

            if (isBillboard) {
                Matrix4x4 scaleMatrix = MakeScaleMatrix(particleIterator->transform.scale);
                Matrix4x4 translateMatrix = MakeTranslateMatrix(particleIterator->transform.translate);

                worldMatrix = Multiply(scaleMatrix, billboardMatrix);
                worldMatrix = Multiply(worldMatrix, translateMatrix);

            } else {
                worldMatrix = MakeAffineMatrix(
                    particleIterator->transform.scale,
                    particleIterator->transform.rotate,
                    particleIterator->transform.translate
                );
            }

            Matrix4x4 worldViewMatrix = Multiply(worldMatrix, viewMatrix);
            Matrix4x4 worldViewProjectionMatrix = Multiply(worldViewMatrix, projectionMatrix);

            if (particleGroup.numInstance >= kNumMaxInstance) {
                break;
            }

            particleGroup.instancingData[particleGroup.numInstance].World = worldMatrix;
            particleGroup.instancingData[particleGroup.numInstance].WVP = worldViewProjectionMatrix;
            particleGroup.instancingData[particleGroup.numInstance].color = particleIterator->color;

            ++particleGroup.numInstance;
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
}

bool ParticleManager::IsCollision(const math::AABB &aabb, const math::Vector3 &point)
{
    if (point.x < aabb.min.x || point.x > aabb.max.x) return false;
    if (point.y < aabb.min.y || point.y > aabb.max.y) return false;
    if (point.z < aabb.min.z || point.z > aabb.max.z) return false;

    return true;
}