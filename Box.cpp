#include "Box.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <common/model.h>

using namespace glm;

Box::Box(float s) {
    size = s;
    cube = new Drawable("models/cube.obj");
}

Box::~Box() {
    delete cube;
}

void Box::draw(unsigned int drawable) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDisable(GL_CULL_FACE);
    cube->bind();
    cube->draw();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_CULL_FACE);
}

void Box::update(float t, float dt) {
    mat4 translate = glm::translate(mat4(), vec3(size / 2, size / 2, size / 2));
    mat4 scale = glm::scale(mat4(), vec3(size, size, size));
    modelMatrix = translate * scale;
}