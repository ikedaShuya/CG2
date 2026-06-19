#pragma once
#include "Object3d.h"

class Skybox
{
public:
	void Initialize(Object3dCommon* object3dCommon);
	void Update();
	void Draw();

private:
	Object3d object3d_;
};