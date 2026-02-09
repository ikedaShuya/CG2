#pragma once
#include "MathTypes.h"

namespace math
{
    // 単位行列
    Matrix4x4 MakeIdentity4x4();

    // 平行移動行列
    Matrix4x4 MakeTranslateMatrix(const Vector3 &translate);

    // 拡大縮小行列
    Matrix4x4 MakeScaleMatrix(const Vector3 &scale);

    // X軸回転行列
    Matrix4x4 MakeRotateXMatrix(float radian);

    // Y軸回転行列
    Matrix4x4 MakeRotateYMatrix(float radian);

    // Z軸回転行列
    Matrix4x4 MakeRotateZMatrix(float radian);

    // 行列の掛け算
    Matrix4x4 Multiply(const Matrix4x4 &m1, const Matrix4x4 &m2);

    // 3次元アフィン変換行列
    Matrix4x4 MakeAffineMatrix(const Vector3 &scale, const Vector3 &rotate, const Vector3 &translate);

    // 逆行列
    Matrix4x4 Inverse(const Matrix4x4 &m);

    // 透視投影行列
    Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);

    // 正射影行列
    Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);

    // 座標変換
    Vector3 ApplyTransform(const Vector3 &vector, const Matrix4x4 &matrix);

    // 正規化
    Vector3 Normalize(const Vector3 &v);

    // 行列の掛け算演算子
    Matrix4x4 operator*(const Matrix4x4 &m1, const Matrix4x4 &m2);
    Vector2 &operator+=(Vector2 &lhs, const Vector2 &rhs);
}