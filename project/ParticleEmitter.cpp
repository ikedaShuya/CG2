#include "ParticleEmitter.h"
#include "ParticleManager.h"

using namespace math;

ParticleEmitter::ParticleEmitter(const std::string& name, const Vector3& position, uint32_t count, float frequency)
{
    name_ = name;
    transform_.translate = position;
    count_ = count;
    frequency_ = frequency;
    frequencyTime_ = 0.0f;
}

void ParticleEmitter::Update(float deltaTime)
{
    frequencyTime_ += deltaTime;

    if (frequencyTime_ >= frequency_)
    {
        Emit();
        frequencyTime_ -= frequency_;
    }
}

void ParticleEmitter::Emit()
{
    ParticleManager::GetInstance()->Emit(name_, transform_.translate, count_);
}

void ParticleEmitter::SetPosition(const Vector3& position)
{
    transform_.translate = position;
}