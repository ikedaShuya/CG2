#include "MathFunctions.h"
#include <cmath>
#include <cassert>

namespace math {

	Matrix4x4 MakeIdentity4x4()
	{
		Matrix4x4 result {};

		for (int i = 0; i < 4; ++i) {
			result.m[i][i] = 1.0f;
		}

		return result;
	}

	Matrix4x4 MakeTranslateMatrix(const Vector3 &translate)
	{
		Matrix4x4 result {};
		result.m[0][0] = 1.0f;
		result.m[0][1] = 0.0f;
		result.m[0][2] = 0.0f;
		result.m[0][3] = 0.0f;

		result.m[1][0] = 0.0f;
		result.m[1][1] = 1.0f;
		result.m[1][2] = 0.0f;
		result.m[1][3] = 0.0f;

		result.m[2][0] = 0.0f;
		result.m[2][1] = 0.0f;
		result.m[2][2] = 1.0f;
		result.m[2][3] = 0.0f;

		result.m[3][0] = translate.x;
		result.m[3][1] = translate.y;
		result.m[3][2] = translate.z;
		result.m[3][3] = 1.0f;

		return result;
	}

	Matrix4x4 MakeScaleMatrix(const Vector3 &scale)
	{
		Matrix4x4 result {};
		result.m[0][0] = scale.x;
		result.m[0][1] = 0.0f;
		result.m[0][2] = 0.0f;
		result.m[0][3] = 0.0f;

		result.m[1][0] = 0.0f;
		result.m[1][1] = scale.y;
		result.m[1][2] = 0.0f;
		result.m[1][3] = 0.0f;

		result.m[2][0] = 0.0f;
		result.m[2][1] = 0.0f;
		result.m[2][2] = scale.z;
		result.m[2][3] = 0.0f;

		result.m[3][0] = 0.0f;
		result.m[3][1] = 0.0f;
		result.m[3][2] = 0.0f;
		result.m[3][3] = 1.0f;

		return result;
	}

	Matrix4x4 MakeRotateXMatrix(float radian)
	{
		Matrix4x4 result {};

		result.m[0][0] = 1.0f;
		result.m[0][1] = 0.0f;
		result.m[0][2] = 0.0f;
		result.m[0][3] = 0.0f;

		result.m[1][0] = 0.0f;
		result.m[1][1] = std::cos(radian);
		result.m[1][2] = std::sin(radian);
		result.m[1][3] = 0.0f;

		result.m[2][0] = 0.0f;
		result.m[2][1] = -(std::sin(radian));
		result.m[2][2] = std::cos(radian);
		result.m[2][3] = 0.0f;

		result.m[3][0] = 0.0f;
		result.m[3][1] = 0.0f;
		result.m[3][2] = 0.0f;
		result.m[3][3] = 1.0f;

		return result;
	}

	Matrix4x4 MakeRotateYMatrix(float radian)
	{
		Matrix4x4 result {};

		result.m[0][0] = std::cos(radian);
		result.m[0][1] = 0.0f;
		result.m[0][2] = -(std::sin(radian));
		result.m[0][3] = 0.0f;

		result.m[1][0] = 0.0f;
		result.m[1][1] = 1.0f;
		result.m[1][2] = 0.0f;
		result.m[1][3] = 0.0f;

		result.m[2][0] = std::sin(radian);
		result.m[2][1] = 0.0f;
		result.m[2][2] = std::cos(radian);
		result.m[2][3] = 0.0f;

		result.m[3][0] = 0.0f;
		result.m[3][1] = 0.0f;
		result.m[3][2] = 0.0f;
		result.m[3][3] = 1.0f;

		return result;
	}

	Matrix4x4 MakeRotateZMatrix(float radian)
	{
		Matrix4x4 result {};

		result.m[0][0] = std::cos(radian);
		result.m[0][1] = std::sin(radian);
		result.m[0][2] = 0.0f;
		result.m[0][3] = 0.0f;

		result.m[1][0] = -(std::sin(radian));
		result.m[1][1] = std::cos(radian);
		result.m[1][2] = 0.0f;
		result.m[1][3] = 0.0f;

		result.m[2][0] = 0.0f;
		result.m[2][1] = 0.0f;
		result.m[2][2] = 1.0f;
		result.m[2][3] = 0.0f;

		result.m[3][0] = 0.0f;
		result.m[3][1] = 0.0f;
		result.m[3][2] = 0.0f;
		result.m[3][3] = 1.0f;

		return result;
	}

	Matrix4x4 Multiply(const Matrix4x4 &m1, const Matrix4x4 &m2)
	{
		Matrix4x4 result {};

		for (int row = 0; row < 4; ++row) {
			for (int col = 0; col < 4; ++col) {
				result.m[row][col] = 0.0f;
				for (int k = 0; k < 4; ++k) {
					result.m[row][col] += m1.m[row][k] * m2.m[k][col];
				}
			}
		}

		return result;
	}

	Matrix4x4 MakeAffineMatrix(const Vector3 &scale, const Vector3 &rotate, const Vector3 &translate)
	{
		Matrix4x4 result {};

		Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);

		Matrix4x4 rotateXMatrix = MakeRotateXMatrix(rotate.x);
		Matrix4x4 rotateYMatrix = MakeRotateYMatrix(rotate.y);
		Matrix4x4 rotateZMatrix = MakeRotateZMatrix(rotate.z);
		Matrix4x4 rotateXYZMatrix =
			Multiply(rotateXMatrix, Multiply(rotateYMatrix, rotateZMatrix));

		Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);

		result = Multiply(Multiply(scaleMatrix, rotateXYZMatrix), translateMatrix);

		return result;
	}

	Matrix4x4 Inverse(const Matrix4x4 &m)
	{
		Matrix4x4 result {};

		float determinant = (m.m[0][0] * m.m[1][1] * m.m[2][2] * m.m[3][3]) +
			(m.m[0][0] * m.m[1][2] * m.m[2][3] * m.m[3][1]) +
			(m.m[0][0] * m.m[1][3] * m.m[2][1] * m.m[3][2]) -

			(m.m[0][0] * m.m[1][3] * m.m[2][2] * m.m[3][1]) -
			(m.m[0][0] * m.m[1][2] * m.m[2][1] * m.m[3][3]) -
			(m.m[0][0] * m.m[1][1] * m.m[2][3] * m.m[3][2]) -

			(m.m[0][1] * m.m[1][0] * m.m[2][2] * m.m[3][3]) -
			(m.m[0][2] * m.m[1][0] * m.m[2][3] * m.m[3][1]) -
			(m.m[0][3] * m.m[1][0] * m.m[2][1] * m.m[3][2]) +

			(m.m[0][3] * m.m[1][0] * m.m[2][2] * m.m[3][1]) +
			(m.m[0][2] * m.m[1][0] * m.m[2][1] * m.m[3][3]) +
			(m.m[0][1] * m.m[1][0] * m.m[2][3] * m.m[3][2]) +

			(m.m[0][1] * m.m[1][2] * m.m[2][0] * m.m[3][3]) +
			(m.m[0][2] * m.m[1][3] * m.m[2][0] * m.m[3][1]) +
			(m.m[0][3] * m.m[1][1] * m.m[2][0] * m.m[3][2]) -

			(m.m[0][3] * m.m[1][2] * m.m[2][0] * m.m[3][1]) -
			(m.m[0][2] * m.m[1][1] * m.m[2][0] * m.m[3][3]) -
			(m.m[0][1] * m.m[1][3] * m.m[2][0] * m.m[3][2]) -

			(m.m[0][1] * m.m[1][2] * m.m[2][3] * m.m[3][0]) -
			(m.m[0][2] * m.m[1][3] * m.m[2][1] * m.m[3][0]) -
			(m.m[0][3] * m.m[1][1] * m.m[2][2] * m.m[3][0]) +

			(m.m[0][3] * m.m[1][2] * m.m[2][1] * m.m[3][0]) +
			(m.m[0][2] * m.m[1][1] * m.m[2][3] * m.m[3][0]) +
			(m.m[0][1] * m.m[1][3] * m.m[2][2] * m.m[3][0]);

		assert(determinant != 0);
		float determinantRecp = 1.0f / determinant;

		result.m[0][0] =
			determinantRecp *
			(m.m[1][1] * m.m[2][2] * m.m[3][3] + m.m[1][2] * m.m[2][3] * m.m[3][1] +
				m.m[1][3] * m.m[2][1] * m.m[3][2] - m.m[1][3] * m.m[2][2] * m.m[3][1] -
				m.m[1][2] * m.m[2][1] * m.m[3][3] - m.m[1][1] * m.m[2][3] * m.m[3][2]);

		result.m[0][1] =
			determinantRecp *
			(-(m.m[0][1] * m.m[2][2] * m.m[3][3]) -
				m.m[0][2] * m.m[2][3] * m.m[3][1] - m.m[0][3] * m.m[2][1] * m.m[3][2] +
				m.m[0][3] * m.m[2][2] * m.m[3][1] + m.m[0][2] * m.m[2][1] * m.m[3][3] +
				m.m[0][1] * m.m[2][3] * m.m[3][2]);

		result.m[0][2] =
			determinantRecp *
			(m.m[0][1] * m.m[1][2] * m.m[3][3] + m.m[0][2] * m.m[1][3] * m.m[3][1] +
				m.m[0][3] * m.m[1][1] * m.m[3][2] - m.m[0][3] * m.m[1][2] * m.m[3][1] -
				m.m[0][2] * m.m[1][1] * m.m[3][3] - m.m[0][1] * m.m[1][3] * m.m[3][2]);

		result.m[0][3] =
			determinantRecp *
			(-(m.m[0][1] * m.m[1][2] * m.m[2][3]) -
				m.m[0][2] * m.m[1][3] * m.m[2][1] - m.m[0][3] * m.m[1][1] * m.m[2][2] +
				m.m[0][3] * m.m[1][2] * m.m[2][1] + m.m[0][2] * m.m[1][1] * m.m[2][3] +
				m.m[0][1] * m.m[1][3] * m.m[2][2]);

		result.m[1][0] =
			determinantRecp *
			(-(m.m[1][0] * m.m[2][2] * m.m[3][3]) -
				m.m[1][2] * m.m[2][3] * m.m[3][0] - m.m[1][3] * m.m[2][0] * m.m[3][2] +
				m.m[1][3] * m.m[2][2] * m.m[3][0] + m.m[1][2] * m.m[2][0] * m.m[3][3] +
				m.m[1][0] * m.m[2][3] * m.m[3][2]);

		result.m[1][1] =
			determinantRecp *
			(m.m[0][0] * m.m[2][2] * m.m[3][3] + m.m[0][2] * m.m[2][3] * m.m[3][0] +
				m.m[0][3] * m.m[2][0] * m.m[3][2] - m.m[0][3] * m.m[2][2] * m.m[3][0] -
				m.m[0][2] * m.m[2][0] * m.m[3][3] - m.m[0][0] * m.m[2][3] * m.m[3][2]);

		result.m[1][2] =
			determinantRecp *
			(-(m.m[0][0] * m.m[1][2] * m.m[3][3]) -
				m.m[0][2] * m.m[1][3] * m.m[3][0] - m.m[0][3] * m.m[1][0] * m.m[3][2] +
				m.m[0][3] * m.m[1][2] * m.m[3][0] + m.m[0][2] * m.m[1][0] * m.m[3][3] +
				m.m[0][0] * m.m[1][3] * m.m[3][2]);

		result.m[1][3] =
			determinantRecp *
			(m.m[0][0] * m.m[1][2] * m.m[2][3] + m.m[0][2] * m.m[1][3] * m.m[2][0] +
				m.m[0][3] * m.m[1][0] * m.m[2][2] - m.m[0][3] * m.m[1][2] * m.m[2][0] -
				m.m[0][2] * m.m[1][0] * m.m[2][3] - m.m[0][0] * m.m[1][3] * m.m[2][2]);

		result.m[2][0] =
			determinantRecp *
			(m.m[1][0] * m.m[2][1] * m.m[3][3] + m.m[1][1] * m.m[2][3] * m.m[3][0] +
				m.m[1][3] * m.m[2][0] * m.m[3][1] - m.m[1][3] * m.m[2][1] * m.m[3][0] -
				m.m[1][1] * m.m[2][0] * m.m[3][3] - m.m[1][0] * m.m[2][3] * m.m[3][1]);

		result.m[2][1] =
			determinantRecp *
			(-(m.m[0][0] * m.m[2][1] * m.m[3][3]) -
				m.m[0][1] * m.m[2][3] * m.m[3][0] - m.m[0][3] * m.m[2][0] * m.m[3][1] +
				m.m[0][3] * m.m[2][1] * m.m[3][0] + m.m[0][1] * m.m[2][0] * m.m[3][3] +
				m.m[0][0] * m.m[2][3] * m.m[3][1]);

		result.m[2][2] =
			determinantRecp *
			(m.m[0][0] * m.m[1][1] * m.m[3][3] + m.m[0][1] * m.m[1][3] * m.m[3][0] +
				m.m[0][3] * m.m[1][0] * m.m[3][1] - m.m[0][3] * m.m[1][1] * m.m[3][0] -
				m.m[0][1] * m.m[1][0] * m.m[3][3] - m.m[0][0] * m.m[1][3] * m.m[3][1]);

		result.m[2][3] =
			determinantRecp *
			(-(m.m[0][0] * m.m[1][1] * m.m[2][3]) -
				m.m[0][1] * m.m[1][3] * m.m[2][0] - m.m[0][3] * m.m[1][0] * m.m[2][1] +
				m.m[0][3] * m.m[1][1] * m.m[2][0] + m.m[0][1] * m.m[1][0] * m.m[2][3] +
				m.m[0][0] * m.m[1][3] * m.m[2][1]);

		result.m[3][0] =
			determinantRecp *
			(-(m.m[1][0] * m.m[2][1] * m.m[3][2]) -
				m.m[1][1] * m.m[2][2] * m.m[3][0] - m.m[1][2] * m.m[2][0] * m.m[3][1] +
				m.m[1][2] * m.m[2][1] * m.m[3][0] + m.m[1][1] * m.m[2][0] * m.m[3][2] +
				m.m[1][0] * m.m[2][2] * m.m[3][1]);

		result.m[3][1] =
			determinantRecp *
			(m.m[0][0] * m.m[2][1] * m.m[3][2] + m.m[0][1] * m.m[2][2] * m.m[3][0] +
				m.m[0][2] * m.m[2][0] * m.m[3][1] - m.m[0][2] * m.m[2][1] * m.m[3][0] -
				m.m[0][1] * m.m[2][0] * m.m[3][2] - m.m[0][0] * m.m[2][2] * m.m[3][1]);

		result.m[3][2] =
			determinantRecp *
			(-(m.m[0][0] * m.m[1][1] * m.m[3][2]) -
				m.m[0][1] * m.m[1][2] * m.m[3][0] - m.m[0][2] * m.m[1][0] * m.m[3][1] +
				m.m[0][2] * m.m[1][1] * m.m[3][0] + m.m[0][1] * m.m[1][0] * m.m[3][2] +
				m.m[0][0] * m.m[1][2] * m.m[3][1]);

		result.m[3][3] =
			determinantRecp *
			(m.m[0][0] * m.m[1][1] * m.m[2][2] + m.m[0][1] * m.m[1][2] * m.m[2][0] +
				m.m[0][2] * m.m[1][0] * m.m[2][1] - m.m[0][2] * m.m[1][1] * m.m[2][0] -
				m.m[0][1] * m.m[1][0] * m.m[2][2] - m.m[0][0] * m.m[1][2] * m.m[2][1]);

		return result;
	}

	Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip)
	{
		Matrix4x4 result {};

		result.m[0][0] = (1.0f / aspectRatio) * (1.0f / std::tan(fovY / 2.0f));
		result.m[0][1] = 0.0f;
		result.m[0][2] = 0.0f;
		result.m[0][3] = 0.0f;

		result.m[1][0] = 0.0f;
		result.m[1][1] = (1.0f / std::tan(fovY / 2.0f));
		result.m[1][2] = 0.0f;
		result.m[1][3] = 0.0f;

		result.m[2][0] = 0.0f;
		result.m[2][1] = 0.0f;
		result.m[2][2] = farClip / (farClip - nearClip);
		result.m[2][3] = 1.0f;

		result.m[3][0] = 0.0f;
		result.m[3][1] = 0.0f;
		result.m[3][2] = (-(nearClip)*farClip) / (farClip - nearClip);
		result.m[3][3] = 0.0f;

		return result;
	}

	Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip)
	{
		Matrix4x4 result {};

		result.m[0][0] = 2.0f / (right - left);
		result.m[0][1] = 0.0f;
		result.m[0][2] = 0.0f;
		result.m[0][3] = 0.0f;

		result.m[1][0] = 0.0f;
		result.m[1][1] = 2.0f / (top - bottom);
		result.m[1][2] = 0.0f;
		result.m[1][3] = 0.0f;

		result.m[2][0] = 0.0f;
		result.m[2][1] = 0.0f;
		result.m[2][2] = 1.0f / (farClip - nearClip);
		result.m[2][3] = 0.0f;

		result.m[3][0] = (left + right) / (left - right);
		result.m[3][1] = (top + bottom) / (bottom - top);
		result.m[3][2] = nearClip / (nearClip - farClip);
		result.m[3][3] = 1.0f;

		return result;
	}

	Vector3 ApplyTransform(const Vector3 &vector, const Matrix4x4 &matrix)
	{
		Vector3 result {};
		result.x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0] + 1.0f * matrix.m[3][0];
		result.y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1] + 1.0f * matrix.m[3][1];
		result.z = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2] + 1.0f * matrix.m[3][2];
		float w = vector.x * matrix.m[0][3] + vector.y * matrix.m[1][3] + vector.z * matrix.m[2][3] + 1.0f * matrix.m[3][3];
		assert(w != 0.0f);
		result.x /= w;
		result.y /= w;
		result.z /= w;

		return result;
	}

	Matrix4x4 operator*(const Matrix4x4 &m1, const Matrix4x4 &m2)
	{
		return Multiply(m1, m2);
	}

}