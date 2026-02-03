#include "Camera.h"
#include "WinApp.h"

using namespace math;

void Camera::Update() {
	// カメラの位置・回転・スケールからワールド行列を作る
	worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);

	// ワールド行列の逆行列をビュー行列にする
	viewMatrix = Inverse(worldMatrix);

	projectionMatrix = MakePerspectiveFovMatrix(fovY, aspectRatio, nearZ, farZ);

	viewProjectionMatrix = Multiply(viewMatrix, projectionMatrix);
}

Camera::Camera()
	: transform({ {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} })
	, fovY(0.45f)
	, aspectRatio(float(WinApp::kClientWidth) / float(WinApp::kClientHeight))
	, nearZ(0.1f)
	, farZ(100.0f)
	, worldMatrix(MakeAffineMatrix(transform.scale, transform.rotate, transform.translate))
	, viewMatrix(Inverse(worldMatrix))
	, projectionMatrix(MakePerspectiveFovMatrix(fovY, aspectRatio, nearZ, farZ))
	, viewProjectionMatrix(Multiply(viewMatrix, projectionMatrix))
{
}