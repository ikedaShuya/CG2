#include "Object3d.h"
#include "Object3dCommon.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
#include <fstream>
#include <sstream>
#include <cassert>

using namespace math;

void Object3d::Initialize(Object3dCommon *object3dCommon)
{
	// ===== 共通部の保持 =====
	this->object3dCommon = object3dCommon;

	// ===== モデル読み込み =====
	modelData = LoadObjFile("resources/models/plane", "plane.obj");

	// ===== GPUリソース生成 =====
	CreateVertexBuffer(modelData.vertices);
	CreateMaterialResource();
	CreateTransformationMatrixResource();
	CreateDirectionalLight();

	// ===== テクスチャ読み込み =====
	TextureManager::GetInstance()->LoadTexture(modelData.material.textureFilePath);
	modelData.material.textureIndex =
		TextureManager::GetInstance()->GetTextureIndexByFilePath(
			modelData.material.textureFilePath);

	// ===== Transform初期化 =====
	transform = {
		{1.0f, 1.0f, 1.0f},   // scale
		{0.0f, 0.0f, 0.0f},   // rotate
		{0.0f, 0.0f, 0.0f}    // translate
	};

	cameraTransform = {
		{1.0f, 1.0f, 1.0f},   // scale
		{0.3f, 0.0f, 0.0f},   // rotate
		{0.0f, 4.0f, -10.0f}  // translate
	};
}

void Object3d::Update()
{
	// ===== ワールド行列 =====
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);

	// ===== ビュー行列（カメラ） =====
	Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
	Matrix4x4 viewMatrix = Inverse(cameraMatrix);

	// ===== 射影行列 =====
	Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClientHeight), 0.1f, 100.0f);

	// ===== 定数バッファへ反映 =====
	transformationMatrixData->WVP = worldMatrix * viewMatrix * projectionMatrix;
	transformationMatrixData->World = worldMatrix;
}

void Object3d::Draw()
{
	// ===== 頂点バッファ設定 =====
	object3dCommon->GetDxCommon()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);

	// ===== 定数バッファ設定 =====
	object3dCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
	object3dCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResource->GetGPUVirtualAddress());
	object3dCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());

	// ===== テクスチャ（SRV）設定 =====
	object3dCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(modelData.material.textureIndex));

	// ===== 描画 =====
	object3dCommon->GetDxCommon()->GetCommandList()->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);
}

Object3d::MaterialData Object3d::LoadMaterialTemplateFile(const std::string &directoryPath, const std::string &filename)
{
	//----中で必要となる変数の宣言----
	MaterialData materialData; // 構築するMaterialData
	std::string line; // ファイルから読んだ1行を格納するもの

	//----ファイルを開く----
	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // とりあえず開かなかったら止める

	//----ファイルを読み込んで、MaterialDataを構築する----
	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		// identiffierに応じた処理
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			// 連結してファイルパスにする
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}

	//----MaterialDataを返す----
	return materialData;
}

Object3d::ModelData Object3d::LoadObjFile(const std::string &directoryPath, const std::string &filename)
{
	//----必要な変数宣言----
	ModelData modelData; // 構築するModelData
	std::vector<math::Vector4> positions; // 位置
	std::vector<math::Vector3> normals; // 法線
	std::vector<math::Vector2> texcoords; // テクスチャ座標
	std::string line; // ファイルから読んだ1行を格納するもの

	//----ファイルを開く----
	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // とりあえず開けなかったら止める

	//----ファイルを読み込んで、ModelDataを構築していく----
	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier; // 先頭の識別子を読む

		// identifierに応じた処理
		if (identifier == "v") {
			math::Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.x *= 1.0f;
			position.w = 1.0f;
			positions.push_back(position);
		} else if (identifier == "vt") {
			math::Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;
			texcoords.push_back(texcoord);
		} else if (identifier == "vn") {
			math::Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normal.x *= -1.0f;
			normals.push_back(normal);
		} else if (identifier == "f") {

			VertexData triangle[3];
			// 面は三角形限定。その他は未対応
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;
				// 頂点の要素へのIndexは「位置/UV/法線」で格納されているので、分解してIndexを取得する
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];
				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/'); // /区切りでインデックスを読んでいく
					elementIndices[element] = std::stoi(index);
				}
				// 要素へのIndexから、実際の要素の値を取得して、頂点を構築する
				math::Vector4 position = positions[elementIndices[0] - 1];
				math::Vector2 texcoord = texcoords[elementIndices[1] - 1];
				math::Vector3 normal = normals[elementIndices[2] - 1];
				triangle[faceVertex] = { position,texcoord,normal };
			}
			// 頂点を逆順で登録することで、回り順を逆にする
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
		} else if (identifier == "mtllib") {
			// materialTemplateLibraryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename;
			// 基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイル名を渡す
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}

	//----ModelDataを返す----
	return modelData;
}

void Object3d::CreateVertexBuffer(const std::vector<VertexData> &vertices)
{
	// 頂点バッファ作成
	vertexResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * vertices.size());

	// VBV設定
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * vertices.size());
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	// 頂点データ転送
	vertexResource->Map(0, nullptr, reinterpret_cast<void **>(&vertexData));
	std::memcpy(vertexData, vertices.data(), sizeof(VertexData) * vertices.size());
}

void Object3d::CreateMaterialResource()
{
	// マテリアル用バッファ作成
	materialResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(Material));

	// 書き込みアドレス取得
	materialResource->Map(0, nullptr, reinterpret_cast<void **>(&materialData));

	// 初期値設定
	materialData->color = math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = false;
	materialData->uvTransform = math::MakeIdentity4x4();
}

void Object3d::CreateTransformationMatrixResource()
{
	// 座標変換行列用リソース作成
	transformationMatrixResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));

	// 書き込み用アドレス取得
	transformationMatrixResource->Map(0, nullptr, reinterpret_cast<void **>(&transformationMatrixData));

	// 単位行列で初期化
	transformationMatrixData->WVP = math::MakeIdentity4x4();
	transformationMatrixData->World = math::MakeIdentity4x4();
}

void Object3d::CreateDirectionalLight()
{
	// 平行光源用バッファ作成
	directionalLightResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(DirectionalLight));

	// 書き込みアドレス取得
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void **>(&directionalLightData));

	// 初期値設定
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->intensity = 1.0f;
}