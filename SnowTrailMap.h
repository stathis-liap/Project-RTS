#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>

class SnowTrailMap {
public:
    SnowTrailMap(int width, int height);
    ~SnowTrailMap();

    void bindForWriting();
    void bindForReading(GLenum textureUnit);
    GLuint getTexture() const { return texture; }

    // Clear trails (e.g. new snow fall)
    void clear();

private:
    GLuint fbo = 0;
    GLuint texture = 0;
    int width_, height_;
};