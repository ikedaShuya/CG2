#pragma once

namespace math
{
    struct Vector2
    {
        float x;
        float y;
    };

    struct Vector3
    {
        float x;
        float y;
        float z;
    };

    struct Vector4
    {
        float x;
        float y;
        float z;
        float w;
    };

    struct Matrix3x3
    {
        float m[3][3];
    };

    struct Matrix4x4
    {
        float m[4][4];
    };

    struct Transform
    {
        Vector3 scale;
        Vector3 rotate;
        Vector3 translate;
    };
}