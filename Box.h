#ifndef BOX_H
#define BOX_H

#include <glm/glm.hpp>

class Drawable;

/**
 * Represents the bounding box
 */
class Box {
public:
    Drawable* cube;
    float size;
    glm::mat4 modelMatrix;

    Box(float s);
    ~Box();

    void draw(unsigned int drawable = 0);
    void update(float t = 0, float dt = 0);
};

#endif
