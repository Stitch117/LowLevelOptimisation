#pragma once

#include "Vec3.h"
#include <list>
#include <GL/glut.h>
#include "globals.h"

class ColliderObject
{
public:
    Vec3 position;
    Vec3 size;
    Vec3 velocity;
    Vec3 colour;

    // if two colliders collide, push them away from each other
    void resolveCollision(ColliderObject* a, ColliderObject* b) {
        Vec3 normal = { a->position.x - b->position.x, a->position.y - b->position.y, a->position.z - b->position.z };
        float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);

        // Normalize the normal vector
        normal.normalise();

        float relativeVelocityX = a->velocity.x - b->velocity.x;
        float relativeVelocityY = a->velocity.y - b->velocity.y;
        float relativeVelocityZ = a->velocity.z - b->velocity.z;

        // Compute the relative velocity along the normal
        float impulse = relativeVelocityX * normal.x + relativeVelocityY * normal.y + relativeVelocityZ * normal.z;

        // Ignore collision if objects are moving away from each other
        if (impulse > 0) {
            return;
        }

        // Compute the collision impulse scalar
        float e = 0.01f; // Coefficient of restitution (0 = inelastic, 1 = elastic)
        float dampening = 0.9f; // Dampening factor (0.9 = 10% energy reduction)
        float j = -(1.0f + e) * impulse * dampening;

        // Apply the impulse to the colliders' velocities
        a->velocity.x += j * normal.x;
        a->velocity.y += j * normal.y;
        a->velocity.z += j * normal.z;
        b->velocity.x -= j * normal.x;
        b->velocity.y -= j * normal.y;
        b->velocity.z -= j * normal.z;
    }

    // are two colliders colliding?
    bool checkCollision(const ColliderObject* a, const ColliderObject* b) {
        return (std::abs(a->position.x - b->position.x) * 2 < (a->size.x + b->size.x)) &&
            (std::abs(a->position.y - b->position.y) * 2 < (a->size.y + b->size.y)) &&
            (std::abs(a->position.z - b->position.z) * 2 < (a->size.z + b->size.z));
    }

    // draw the physics object
    void draw() {
        glPushMatrix();
        glTranslatef(position.x, position.y, position.z);
        GLfloat diffuseMaterial[] = { colour.x, colour.y, colour.z, 1.0f };
        glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuseMaterial);
        glScalef(size.x, size.y, size.z);
        glRotatef(-90, 1, 0, 0);
        drawMesh();
        //glutSolidTeapot(1);
        //glutSolidCone(1, 1, 10, 10);
        glPopMatrix();
    }

    virtual void drawMesh() {};

    void update(std::list<ColliderObject*>* colliders, const float& deltaTime)
    {
        const float floorY = 0.0f;
        // Update velocity due to gravity
        velocity.y += gravity * deltaTime;

        // Update position based on velocity
        position.x += velocity.x * deltaTime;
        position.y += velocity.y * deltaTime;
        position.z += velocity.z * deltaTime;

        // Check for collision with the floor
        if (position.y - size.y / 2.0f < floorY) {
            position.y = floorY + size.y / 2.0f;
            float dampening = 0.7f;
            velocity.y = -velocity.y * dampening;
        }

        // Check for collision with the walls
        if (position.x - size.x / 2.0f < minX || position.x + size.x / 2.0f > maxX) {
            velocity.x = -velocity.x;
        }
        if (position.z - size.z / 2.0f < minZ || position.z + size.z / 2.0f > maxZ) {
            velocity.z = -velocity.z;
        }

        // Check for collisions with other colliders
        for (ColliderObject* other : *colliders) {
            if (this == other) continue;
            if (checkCollision(this, other)) {
                resolveCollision(this, other);
                break;
            }
        }
    }
};

