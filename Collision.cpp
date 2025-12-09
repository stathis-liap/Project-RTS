#include "Collision.h"
#include "Box.h"
#include "Sphere.h"

using namespace glm;

void handleBoxSphereCollision(Box& box, Sphere& sphere);
bool checkForBoxSphereCollision(glm::vec3 &pos, const float& r,
                                const float& size, glm::vec3& n);




void handleBoxSphereCollision(Box& box, Sphere& sphere) {
    vec3 n;
    if (checkForBoxSphereCollision(sphere.x, sphere.r, box.size, n)) {
        // Task 2b: define the velocity of the sphere after the collision
        sphere.v = sphere.v - 2.0f * dot(sphere.v, n) * n;
        sphere.P = sphere.v * sphere.m;
    }
}


bool checkForBoxSphereCollision(vec3 &pos, const float& r,
                                const float& size, vec3& n) {
    if (pos.x - r <= 0) {
        //correction
        float dis = -(pos.x - r);
        pos = pos + vec3(dis, 0, 0);

        n = vec3(-1, 0, 0);
    } else if (pos.x + r >= size) {
        //correction
        float dis = size - (pos.x + r);
        pos = pos + vec3(dis, 0, 0);

        n = vec3(1, 0, 0);
    } else if (pos.y - r <= 0) {
        //correction
        float dis = -(pos.y - r);
        pos = pos + vec3(0, dis, 0);

        n = vec3(0, -1, 0);
    } else if (pos.y + r >= size) {
        //correction
        float dis = size - (pos.y + r);
        pos = pos + vec3(0, dis, 0);

        n = vec3(0, 1, 0);
    } else if (pos.z - r <= 0) {
        //correction
        float dis = -(pos.z - r);
        pos = pos + vec3(0, 0, dis);

        n = vec3(0, 0, -1);
    } else if (pos.z + r >= size) {
        //correction
        float dis = size - (pos.z + r);
        pos = pos + vec3(0, 0, dis);

        n = vec3(0, 0, 1);
    } else {
        return false;
    }

    return true;
}


bool checkSphereSphereCollision(vec3& pos1, const float& r1,vec3& pos2, const float& r2, vec3& n) {
    vec3 d = pos2 - pos1;
    float dist = length(d);
    if (dist <= r1 + r2) {
        float overlap = r1 + r2 - dist;
        float correction = overlap / dist;
        pos1 = pos1 - vec3(correction * 0.33f, correction * 0.33f,correction * 0.33f);
        pos2 = pos2 + vec3(correction * 0.33f, correction * 0.33f,correction * 0.33f);
        n = normalize(d);
        return true;
    }
    return false;
}

void handleSphereSphereCollision(Sphere& sphere1, Sphere& sphere2) {
    vec3 n;

    if (checkSphereSphereCollision(sphere1.x, sphere1.r, sphere2.x, sphere2.r, n)) {
        float m1 = sphere1.m;
        float m2 = sphere2.m;
        vec3 u1 = sphere1.v;
        vec3 u2 = sphere2.v;
        sphere1.v = u1 - (2.0f * m2 / (m1 + m2)) * dot(u1 - u2, n) * n;
        sphere2.v = u2 - (2.0f * m1 / (m1 + m2)) * dot(u2 - u1, -n) * -n;
        sphere1.P = sphere1.v * sphere1.m;
        sphere2.P = sphere2.v * sphere2.m;
    }

}
