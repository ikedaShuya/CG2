#include "Skybox.h"
#include "Model.h"

void Skybox::Initialize(Object3dCommon* object3dCommon)
{
	object3d_.Initialize(object3dCommon);

	// スカイボックスは大きくする
	object3d_.SetScale({ 100.0f,100.0f,100.0f });
}

void Skybox::Update()
{
	object3d_.Update();
}

void Skybox::Draw()
{
	object3d_.Draw();
}