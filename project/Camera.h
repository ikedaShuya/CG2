#pragma once
#include "MathFunctions.h"

// カメラ
class Camera
{
public: // メンバ関数

	// 更新
	void Update();

	// setter
	void SetRotate(const math::Vector3 &rotate) { transform.rotate = rotate; }
	void SetTranslate(const math::Vector3 &translate) { transform.translate = translate; }
	void SetFovY(float fovY_) { fovY = fovY_; }
	void SetAspectRatio(float aspect) { aspectRatio = aspect; }
	void SetNearClip(float nearClip) { nearZ = nearClip; }
	void SetFarClip(float farClip) { farZ = farClip; }

	// getter
	const math::Matrix4x4 &GetWorldMatrix() const { return worldMatrix; }
	const math::Matrix4x4 &GetViewMatrix() const { return viewMatrix; }
	const math::Matrix4x4 &GetProjectionMatrix() const { return projectionMatrix; }
	const math::Matrix4x4 &GetViewProjectionMatrix() const { return viewProjectionMatrix; }
	const math::Vector3 &GetRotate() const { return transform.rotate; }
	const math::Vector3 &GetTranslate() const { return transform.translate; }
	float GetFovY() const { return fovY; }
	float GetNearClip() const { return nearZ; }
	float GetFarClip() const { return farZ; }

	Camera();

private:
	math::Transform transform;
	math::Matrix4x4 worldMatrix;
	math::Matrix4x4 viewMatrix;

	math::Matrix4x4 projectionMatrix;
	float fovY; // 垂直方向視野角 
	float aspectRatio; // アスペクト比 
	float nearZ; // ニアクリップ距離 
	float farZ; // ファークリップ距離

	math::Matrix4x4 viewProjectionMatrix;
};