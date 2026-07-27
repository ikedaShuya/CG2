#include "object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
};

struct DirectionalLight
{
    float32_t4 color; //!< ライトの色
    float32_t3 direction; //!< ライトの向き
    float intensity; //!< 輝度
};

ConstantBuffer<Material> gMaterial : register(b0);
Texture2D<float32_t4> gTexture : register(t0);

// 環境マップを追加
TextureCube<float32_t4> gEnvironmentTexture : register(t1);

SamplerState gSampler : register(s0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

struct Camera
{
    float32_t3 worldPosition;
    float padding;
};

ConstantBuffer<Camera> gCamera : register(b2);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // ==============================
    // 通常のテクスチャを取得
    // ==============================
    float4 transformedUV =
        mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);

    float32_t4 textureColor =
        gTexture.Sample(gSampler, transformedUV.xy);

    // textureのα値が0.5以下のときにPixelを棄却
    if (textureColor.a <= 0.5)
    {
        discard;
    }

    // ==============================
    // 通常のライティング
    // ==============================
    if (gMaterial.enableLighting != 0)
    {
        // Half Lambert
        float NdotL =
            dot(normalize(input.normal), -gDirectionalLight.direction);

        float cos =
            pow(NdotL * 0.5f + 0.5f, 2.0f);

        output.color.rgb =
            gMaterial.color.rgb *
            textureColor.rgb *
            gDirectionalLight.color.rgb *
            cos *
            gDirectionalLight.intensity;

        output.color.a =
            gMaterial.color.a *
            textureColor.a;
    }
    else
    {
        // Lightingしない場合
        output.color =
            gMaterial.color *
            textureColor;
    }

 // ==============================
// 環境マップによる反射
// ==============================

// カメラからモデルへ向かう方向
    float32_t3 cameraToPosition =
    normalize(input.worldPosition - gCamera.worldPosition);

// 法線を使って反射方向を計算
    float32_t3 reflectedVector =
    reflect(cameraToPosition, normalize(input.normal));

// 反射方向から環境マップの色を取得
    float32_t4 environmentColor =
    gEnvironmentTexture.Sample(gSampler, reflectedVector);

// 環境マップの映り込みを30%にする
    output.color.rgb += environmentColor.rgb * 0.8f;

    return output;
}