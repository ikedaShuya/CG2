#include "Model.h"
#include "ModelCommon.h"
#include "TextureManager.h"
#include "DirectXCommon.h"
#include <fstream>
#include <sstream>
#include <cassert>

void Model::Initialize(ModelCommon *modelCommon)
{
	// ===== 共通部の保持 =====
	this->modelCommon_ = modelCommon;

	// ===== モデル読み込み =====
	modelData_ = LoadObjFile("resources/models/plane", "plane.obj");

	CreateVertexBuffer(modelData_.vertices);
	CreateMaterialResource();

	// ===== テクスチャ読み込み =====
	TextureManager::GetInstance()->LoadTexture(modelData_.material.textureFilePath);
	modelData_.material.textureIndex =
		TextureManager::GetInstance()->GetTextureIndexByFilePath(
			modelData_.material.textureFilePath);
}

void Model::Draw() {

	// ===== 頂点バッファ設定 =====
	modelCommon_->GetDxCommon()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);

	// ===== 定数バッファ設定 =====
	modelCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

	// ===== テクスチャ（SRV）設定 =====
	modelCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(modelData_.material.textureIndex));

	// ===== 描画 =====
	modelCommon_->GetDxCommon()->GetCommandList()->DrawInstanced(UINT(modelData_.vertices.size()), 1, 0, 0);
}

Model::MaterialData Model::LoadMaterialTemplateFile(const std::string &directoryPath, const std::string &filename)
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

Model::ModelData Model::LoadObjFile(const std::string &directoryPath, const std::string &filename)
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

void Model::CreateVertexBuffer(const std::vector<VertexData> &vertices)
{
	// 頂点バッファ作成
	vertexResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * vertices.size());

	// VBV設定
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * vertices.size());
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	// 頂点データ転送
	vertexResource->Map(0, nullptr, reinterpret_cast<void **>(&vertexData));
	std::memcpy(vertexData, vertices.data(), sizeof(VertexData) * vertices.size());
}

void Model::CreateMaterialResource()
{
	// マテリアル用バッファ作成
	materialResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(Material));

	// 書き込みアドレス取得
	materialResource->Map(0, nullptr, reinterpret_cast<void **>(&materialData));

	// 初期値設定
	materialData->color = math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = false;
	materialData->uvTransform = math::MakeIdentity4x4();
}