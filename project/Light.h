#pragma once
#include "MathTypes.h"

namespace light
{
	struct DirectionalLight
	{
		math::Vector4 color; //!< ライトの色
		math::Vector3 direction; //!< ライトの向き
		float intensity; //!< 輝度
	};
}