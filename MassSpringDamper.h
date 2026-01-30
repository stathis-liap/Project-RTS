#ifndef MASS_SPRING_DAMPER_H
#define MASS_SPRING_DAMPER_H

#include "RigidBody.h"

class Drawable;

class MassSpringDamper : public RigidBody {
public:
    Drawable *blob, *spring;
    float b, k, l0, l;
    glm::vec3 a;
    glm::mat4 springModelMatrix, blobModelMatrix;

    MassSpringDamper(glm::vec3 pos, glm::vec3 vel, float length, float mass,
                     glm::vec3 anchor, float stiffness, float damping, float restLength);
    ~MassSpringDamper();

    void draw(unsigned int drawable = 0);
    void update(float t = 0, float dt = 0);
};

#endif