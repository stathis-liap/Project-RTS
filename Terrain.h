#ifndef TERRAIN_H
#define TERRAIN_H
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>

class Terrain {
public:
    Terrain(int width, int height, float amplitude = 1.5f);
    ~Terrain();

    void generate();              // Create heightmap + mesh
    void draw(const glm::mat4& M);

    glm::vec3 getHeightAt(float x, float z) const;

    GLuint vao, vbo, ebo;
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

    void generateHeightmap();
    void computeNormals();
    void buildMesh();
    void applyDiagonalSymmetry();

    float noise(float x, float z) const; // Simple value noise
	float hash(int xi, int zi) const;
};

#endif