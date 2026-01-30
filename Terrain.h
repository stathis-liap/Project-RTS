#ifndef TERRAIN_H
#define TERRAIN_H

#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>

class Terrain {
public:
    Terrain(int width, int height, float amplitude = 1.5f);
    ~Terrain();

    void generate(); // Create heightmap + mesh

    // Normal draw — assumes correct shader is already bound
    void draw(const glm::mat4& model);

    // Overload for shadow pass — explicit shader
    void draw(const glm::mat4& model, GLuint shaderProgram);

    glm::vec3 getHeightAt(float x, float z) const;

    void flattenArea(const glm::vec3& center, float radius);

    int width, height;
    float amplitude;

private:
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 normal;
    };

    std::vector<float> heightmap;
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;

    void generateHeightmap();
    void computeNormals();
    void buildMesh();
    void applyDiagonalSymmetry();
    void recalculateNormalsInArea(int centerX, int centerZ, int radius);

    float noise(float x, float z) const;
    float hash(int xi, int zi) const;
};

#endif