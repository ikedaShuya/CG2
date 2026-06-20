#pragma once
#include <cstdint>
#include <d3d12.h>
#include "MathFunctions.h"

class DirectXCommon;

// 3Dモデル共通部
class ModelCommon
{
public:

	// 座標変換行列
	struct TransformationMatrix
	{
		math::Matrix4x4 WVP;
		math::Matrix4x4 World;
	};

	//初期化
	void Initialize(DirectXCommon *dxCommon);

public:
	DirectXCommon *GetDxCommon() const { return dxCommon_; }

private:

	DirectXCommon *dxCommon_;
};