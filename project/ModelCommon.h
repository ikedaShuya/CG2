#pragma once

class DirectXCommon;

// 3Dモデル共通部
class ModelCommon
{
public:

	//初期化
	void Initialize(DirectXCommon *dxCommon);

public:
	DirectXCommon *GetDxCommon() const { return dxCommon_; }

private:

	DirectXCommon *dxCommon_;
};