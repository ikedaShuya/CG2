#pragma once
#include <string>
#include "MathFunctions.h"

class ParticleEmitter
{
public:

	// コンストラクタ
	ParticleEmitter(const std::string& name, const math::Vector3& position, uint32_t count, float frequency);

	// 更新
	void Update(float deltaTime);

	// パーティクル発生
	void Emit();

	void SetPosition(const math::Vector3& position);

private:

	// パーティクルグループ名
	std::string name_;

	// エミッタのTransform
	math::Transform transform_;

	// 発生数
	uint32_t count_;

	// 発生間隔
	float frequency_;

	// 経過時間
	float frequencyTime_;
};