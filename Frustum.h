#pragma once 

#include <glm/glm.hpp>

struct Plane {
    glm::vec3 normal;
    float distance;

    void normalize() {
        float mag = glm::length(normal);
        if (mag > 0.00001f) {
            normal /= mag;
            distance /= mag;
        }
    }
};

class Frustum {
public:
    Plane planes[6];

    void update(const glm::mat4& mat) {
        // Left
        planes[0].normal.x = mat[0][3] + mat[0][0];
        planes[0].normal.y = mat[1][3] + mat[1][0];
        planes[0].normal.z = mat[2][3] + mat[2][0];
        planes[0].distance = mat[3][3] + mat[3][0];

        // Right
        planes[1].normal.x = mat[0][3] - mat[0][0];
        planes[1].normal.y = mat[1][3] - mat[1][0];
        planes[1].normal.z = mat[2][3] - mat[2][0];
        planes[1].distance = mat[3][3] - mat[3][0];

        // Bottom
        planes[2].normal.x = mat[0][3] + mat[0][1];
        planes[2].normal.y = mat[1][3] + mat[1][1];
        planes[2].normal.z = mat[2][3] + mat[2][1];
        planes[2].distance = mat[3][3] + mat[3][1];

        // Top
        planes[3].normal.x = mat[0][3] - mat[0][1];
        planes[3].normal.y = mat[1][3] - mat[1][1];
        planes[3].normal.z = mat[2][3] - mat[2][1];
        planes[3].distance = mat[3][3] - mat[3][1];

        // Near
        planes[4].normal.x = mat[0][3] + mat[0][2];
        planes[4].normal.y = mat[1][3] + mat[1][2];
        planes[4].normal.z = mat[2][3] + mat[2][2];
        planes[4].distance = mat[3][3] + mat[3][2];

        // Far
        planes[5].normal.x = mat[0][3] - mat[0][2];
        planes[5].normal.y = mat[1][3] - mat[1][2];
        planes[5].normal.z = mat[2][3] - mat[2][2];
        planes[5].distance = mat[3][3] - mat[3][2];

        for (int i = 0; i < 6; i++) planes[i].normalize();
    }

    bool isSphereVisible(const glm::vec3& center, float radius) const {
        for (int i = 0; i < 6; i++) {
            if (glm::dot(planes[i].normal, center) + planes[i].distance < -radius) {
                return false;
            }
        }
        return true;
    }
};