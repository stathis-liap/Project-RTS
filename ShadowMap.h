#pragma once
#include <GL/glew.h>

class ShadowMap {
public:
    ShadowMap(int width = 2048, int height = 2048);
    ~ShadowMap();

    void bindForWriting();
    void bindForReading(GLenum textureUnit);
    GLuint getDepthTexture() const { return depthMap; }

private:
    GLuint FBO;
    GLuint depthMap;
    int width_, height_;
};