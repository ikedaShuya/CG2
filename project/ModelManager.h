#pragma once
#include <map>
#include <string>
#include <memory>

class Model;
class ModelCommon;
class DirectXCommon;

// テクスチャマネージャー
class ModelManager
{
public:

	// 初期化
	void Initialize(DirectXCommon *dxCommon);
	// シングルトンインスタンスの取得
	static ModelManager *GetInstance();
	// 終了
	void Finalize();

	/// <summary>
	/// モデルファイルの読み込み
	/// </summary>
	/// <param name="filePath">モデルファイルのパス</param>
	void LoadModel(const std::string &filePath);

	/// <summary>
	/// モデルの検索
	/// </summary>
	/// <param name="filePath">モデルファイルのパス</param>
	/// <returns>モデル</returns>
	Model* FindModel(const std::string &filePath);

private:
	static ModelManager *instance;

	ModelManager() = default;
	~ModelManager() = default;
	ModelManager(const ModelManager &) = delete;
	ModelManager &operator=(const ModelManager &) = delete;

	// モデルデータ
	std::map<std::string, std::unique_ptr<Model>> models;

	ModelCommon *modelCommon = nullptr;
};