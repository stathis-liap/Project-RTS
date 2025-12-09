#include "MassSpringDamper.h"
#include <glm/gtc/matrix_transform.hpp>
#include <common/model.h>
#include <iostream>

using namespace glm;

MassSpringDamper::MassSpringDamper(
    vec3 pos, vec3 vel, float length, float mass,
    vec3 anchor, float stiffness, float damping, float restLength)
    : a(anchor), k(stiffness), b(damping), l0(restLength) {
    blob = new Drawable("models/cube.obj");
    spring = new Drawable("models/spring.obj");

    l = length;
    m = mass;
    x = pos;
    v = vel;
    P = m * v;

    if (length == 0) throw std::runtime_error("length != 0");
    mat3 I = mat3(
        1.0f / 6 * mass*l*l, 0, 0,
        0, 1.0f / 6 * mass*l*l, 0,
        0, 0, 1.0f / 6 * mass*l*l);
    I_inv = inverse(I);
}

MassSpringDamper::~MassSpringDamper() {
    delete blob;
    delete spring;
}

void MassSpringDamper::draw(unsigned int drawable) {
    if (drawable == 0) {
        spring->bind();
        spring->draw();
    } else if (drawable == 1) {
        blob->bind();
        blob->draw();
    }
}

void MassSpringDamper::update(float t, float dt) {
    //integration
    advanceState(t, dt);

    float springLength = glm::distance(a, x);

    // compute model matrix
    mat4 blobScale = glm::scale(mat4(), vec3(l, l, l));
    mat4 springScale = glm::scale(mat4(), vec3(l, springLength / 2.0, l));
    mat4 tranlation = translate(mat4(), vec3(x.x, x.y, x.z));
#ifdef USE_QUATERNIONS
    mat4 rotation = mat4_cast(q);
#else
    mat4 rotation = mat4(R);
#endif

    springModelMatrix = tranlation * rotation * springScale;
    blobModelMatrix = tranlation * rotation * blobScale;
}