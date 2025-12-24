// Mesh.h
#ifndef MESH_H
#define MESH_H

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <string>

class Mesh {
public:
    explicit Mesh(const std::string& filepath);
    ~Mesh();

    void draw() const;

    // Add this so shadow pass can use it
    GLuint getVAO() const { return VAO; }
    GLsizei getIndexCount() const { return indexCount; }

private:
    GLuint VAO = 0, VBO = 0, EBO = 0;
    GLsizei indexCount = 0;
};

#endif // MESH_H