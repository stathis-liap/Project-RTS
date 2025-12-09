#include "Sphere.h"
#include <glm/gtc/matrix_transform.hpp>
#include <common/model.h>
#include <iostream>

using namespace glm;

Sphere::Sphere(vec3 pos, vec3 vel, float radius, float mass)
    : RigidBody() {
    sphere = new Drawable("models/sphere.obj");

    r = radius;
    m = mass;
    x = pos;
    v = vel;
    P = m * v;

    if (radius == 0) throw std::logic_error("Sphere: radius != 0");
    mat3 I = mat3(
        2.0f / 5 * mass*radius*radius, 0, 0,
        0, 2.0f / 5 * mass*radius*radius, 0,
        0, 0, 2.0f / 5 * mass*radius*radius);
    I_inv = inverse(I);
}

Sphere::~Sphere() {
    delete sphere;
}

void Sphere::draw(unsigned int drawable) {
    sphere->bind();
    sphere->draw();
}

void Sphere::update(float t, float dt) {
    //integration
    advanceState(t, dt);

    // compute model matrix
    mat4 scale = glm::scale(mat4(), vec3(r, r, r));
    mat4 tranlation = translate(mat4(), vec3(x.x, x.y, x.z));
#ifdef USE_QUATERNIONS
    mat4 rotation = mat4_cast(q);
#else
    mat4 rotation = mat4(R);
#endif
    modelMatrix = tranlation * rotation * scale;
}