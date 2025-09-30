#pragma once

#include "ColliderObject.h"

class Box : public ColliderObject
{
public:

	void drawMesh() { glutSolidCube(1.0); }
};

