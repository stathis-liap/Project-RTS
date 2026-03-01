#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <cmath>
#include "camera.h"

using namespace glm;

Camera::Camera(GLFWwindow* win) : window(win)
{
    position = vec3(25.0f, 50.0f, 25.0f);   // start above map center
    horizontalAngle = 0.0f;
    verticalAngle = -1.0f;                 // ~57° tilt down (-1 in rad)
    FoV = 70.0f;
    speed = 2.0f;
    zoomSpeed = 3.0f;
    rotateSpeed = 2.0f;
    minHeight = 5.0f;
    maxHeight = 300.0f;

    // Register scroll callback
    glfwSetWindowUserPointer(window, this);
    glfwSetScrollCallback(window, scrollCallback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    // We want the cursor visible and free for RTS selection.
}


// Scroll callback – zoom toward cursor
void Camera::scrollCallback(GLFWwindow* win, double xoffset, double yoffset)
{
    Camera* cam = static_cast<Camera*>(glfwGetWindowUserPointer(win));
    if (!cam) return;

    double mx, my;
    glfwGetCursorPos(win, &mx, &my);
    vec3 hit = cam->getPlaneIntersect(mx, my, 0.0f);
    float dh = -static_cast<float>(yoffset) * cam->zoomSpeed;
    cam->zoomTowards(hit.x, hit.z, dh);
}


// Get ray from camera through cursor
void Camera::getRayDirAtCursor(double mouseX, double mouseY, vec3& outDir) const
{
    int w, h;
    glfwGetWindowSize(window, &w, &h);

    float ndcX = (2.0f * static_cast<float>(mouseX)) / w - 1.0f;
    float ndcY = 1.0f - (2.0f * static_cast<float>(mouseY)) / h;

    vec4 clip = vec4(ndcX, ndcY, -1.0f, 1.0f);
    vec4 eye = inverseProjection * clip;
    eye = vec4(eye.x, eye.y, eye.z, 0.0f);  // direction

    vec4 world4 = inverseView * eye;
    outDir = normalize(vec3(world4.x, world4.y, world4.z));
}


// Intersect ray with plane Y = planeY
vec3 Camera::getPlaneIntersect(double mouseX, double mouseY, float planeY) const
{
    vec3 rayDir;
    getRayDirAtCursor(mouseX, mouseY, rayDir);

    float denom = rayDir.y;
    if (std::abs(denom) < 1e-6f) return position;

    float t = (planeY - position.y) / denom;
    return (t >= 0.0f) ? position + t * rayDir : position;
}

// Zoom toward world point (XZ), keeping it under cursor
void Camera::zoomTowards(float targetX, float targetZ, float dh)
{
    float oldH = position.y;
    position.y += dh;
    position.y = glm::clamp(position.y, minHeight, maxHeight);

    if (std::abs(oldH - position.y) < 1e-6f) return;

    float scale = position.y / oldH;
    position.x = targetX + (position.x - targetX) * scale;
    position.z = targetZ + (position.z - targetZ) * scale;
}


// Update – input, matrices, cache inverses
void Camera::update()
{
    static double lastTime = glfwGetTime();
    double now = glfwGetTime();
    float dt = static_cast<float>(now - lastTime);
    lastTime = now;

    // SAFETY FIX: If we paused this camera (Unit Mode), dt might be huge (e.g. 10 seconds).
    // If dt is too large, cap it to 1 frame (approx 60fps) to prevent the camera from teleporting.
    if (dt > 0.1f) dt = 0.016f;

    float curSpeed = speed * position.y;

    // Standard Direction Vectors
    vec3 dir(
        cos(verticalAngle) * sin(horizontalAngle),
        sin(verticalAngle),
        cos(verticalAngle) * cos(horizontalAngle)
    );
    vec3 right(
        sin(horizontalAngle - 3.14159f / 2.0f),
        0.0f,
        cos(horizontalAngle - 3.14159f / 2.0f)
    );
    vec3 up = cross(right, dir);

    vec3 fwdH = normalize(vec3(dir.x, 0, dir.z));
    vec3 rightH = normalize(vec3(right.x, 0, right.z));

    // KEYBOARD MOVEMENT (WASD)
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        position.x += fwdH.x * dt * curSpeed;
        position.z += fwdH.z * dt * curSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        position.x -= fwdH.x * dt * curSpeed;
        position.z -= fwdH.z * dt * curSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        position.x += rightH.x * dt * curSpeed;
        position.z += rightH.z * dt * curSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        position.x -= rightH.x * dt * curSpeed;
        position.z -= rightH.z * dt * curSpeed;
    }

    // EDGE PANNING 
    if (glfwGetWindowAttrib(window, GLFW_FOCUSED)) {
        double mx, my;
        int width, height;
        glfwGetCursorPos(window, &mx, &my);
        glfwGetWindowSize(window, &width, &height);
        float edgeMargin = 10.0f;

        if (mx < edgeMargin) { position.x -= rightH.x * dt * curSpeed; position.z -= rightH.z * dt * curSpeed; }
        if (mx > width - edgeMargin) { position.x += rightH.x * dt * curSpeed; position.z += rightH.z * dt * curSpeed; }
        if (my < edgeMargin) { position.x += fwdH.x * dt * curSpeed; position.z += fwdH.z * dt * curSpeed; }
        if (my > height - edgeMargin) { position.x -= fwdH.x * dt * curSpeed; position.z -= fwdH.z * dt * curSpeed; }
    }

    // Update Matrices
    int w, h;
    glfwGetWindowSize(window, &w, &h);
    float aspect = static_cast<float>(w) / h;
    projectionMatrix = perspective(radians(FoV), aspect, 0.1f, 1000.0f);
    viewMatrix = lookAt(position, position + dir, up);

    inverseView = glm::inverse(viewMatrix);
    inverseProjection = glm::inverse(projectionMatrix);
}