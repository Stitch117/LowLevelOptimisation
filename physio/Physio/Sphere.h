#pragma once
#include "ColliderObject.h"
class Sphere :
    public ColliderObject
{
public:

    void drawMesh() { glutSolidSphere(0.5, 5, 5); }
}; 

