#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

class Camera {
public:
    GLFWwindow* window;
    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;

    glm::vec3 position;
    float horizontalAngle;
    float verticalAngle;
    float FoV;
    float speed;
    float zoomSpeed;
    float rotateSpeed;
    float minHeight;
    float maxHeight;

    Camera(GLFWwindow* window);
    void update();

    // For zoom-to-cursor & raycasting
    void getRayDirAtCursor(double mouseX, double mouseY, glm::vec3& rayDir) const;
    glm::vec3 getPlaneIntersect(double mouseX, double mouseY, float planeY = 0.0f) const;
    void zoomTowards(float targetX, float targetZ, float heightDelta);

    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

private:
    // Cached inverses for raycasting performance
    glm::mat4 inverseView;
    glm::mat4 inverseProjection;
};

#endif