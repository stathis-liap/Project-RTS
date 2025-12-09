#ifndef SPHERE_H
#define SPHERE_H

#include "RigidBody.h"

class Drawable;

class Sphere : public RigidBody {
public:
    Drawable* sphere;
    float r;
    glm::mat4 modelMatrix;

    Sphere(glm::vec3 pos, glm::vec3 vel, float radius, float mass);
    ~Sphere();

    void draw(unsigned int drawable = 0);
    void update(float t = 0, float dt = 0);
};

#endif