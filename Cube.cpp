#include "Cube.h"
#include <glm/gtc/matrix_transform.hpp>
#include <common/model.h>
#include <iostream>

using namespace glm;

Cube::Cube(vec3 pos, vec3 vel, vec3 omega, float length, float mass)
    : RigidBody() {
    cube = new Drawable("models/cube.obj");

    l = length;
    m = mass;
    x = pos;
    v = vel;
    P = m * v;
    w = omega;

    if (l == 0) throw std::logic_error("Cube: length != 0");
    mat3 I = mat3(
        1.0f / 6 * mass * l * l, 0, 0,
        0, 1.0f / 6 * mass * l * l, 0,
        0, 0, 1.0f / 6 * mass * l * l);

    L = I * w;
    I_inv = inverse(I);
}

Cube::~Cube() {
    delete cube;
}

void Cube::draw(unsigned int drawable) {
    cube->bind();
    cube->draw();
}

void Cube::update(float t, float dt) {
    // numerical integration
    advanceState(t, dt);

    // compute model matrix
    mat4 scale = glm::scale(mat4(), vec3(l, l, l));
    mat4 tranlation = translate(mat4(), vec3(x.x, x.y, x.z));
#ifdef USE_QUATERNIONS
    mat4 rotation = mat4_cast(q);
#else
    mat4 rotation = mat4(R);
#endif
    modelMatrix = tranlation * rotation * scale;
}