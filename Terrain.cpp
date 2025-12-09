#include "Terrain.h"
#include <algorithm> // std::swap
#include <cmath>
#include <cstdlib>
using namespace glm;

Terrain::Terrain(int width, int height, float amplitude)
    : width(width), height(height), amplitude(amplitude)
{
    generate();
}

Terrain::~Terrain() {
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteVertexArrays(1, &vao);
}

void Terrain::generate() {
    generateHeightmap();
    computeNormals();
    buildMesh();
    applyDiagonalSymmetry();
}

// ------------------------------------------------------------
// Simple procedural value noise (now with smooth interpolation)
// ------------------------------------------------------------
float Terrain::hash(int xi, int zi) const {
    int seed = xi * 49632 + zi * 325176 + 12345;
    seed = (seed << 13) ^ seed;
    float n = (1.0f - ((seed * (seed * seed * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
    return n;
}

float Terrain::noise(float x, float z) const {
    int xi = (int)glm::floor(x);
    int zi = (int)glm::floor(z);
    float fx = glm::fract(x);
    float fz = glm::fract(z);
    // Smoothstep interpolation for softness
    float ux = fx * fx * (3.0f - 2.0f * fx);
    float uz = fz * fz * (3.0f - 2.0f * fz);
    float n00 = hash(xi, zi);
    float n01 = hash(xi, zi + 1);
    float n10 = hash(xi + 1, zi);
    float n11 = hash(xi + 1, zi + 1);
    float nz0 = glm::mix(n00, n01, uz);
    float nz1 = glm::mix(n10, n11, uz);
    return glm::mix(nz0, nz1, ux);
}

// ------------------------------------------------------------
// Procedural heightmap (mirrored for symmetry)
// ------------------------------------------------------------
void Terrain::generateHeightmap() {
    heightmap.resize(width * height);
    // Increased base frequency slightly for shorter bumps, but interpolation makes them soft
    float freq = 0.1f; // Adjust this higher for more frequent/smaller bumps, lower for larger features
    // Added a third octave for finer, softer details
    for (int z = 0; z < height; z++) {
        for (int x = 0; x < width; x++) {
            float h =
                noise(x * freq, z * freq) * amplitude +
                noise(x * freq * 2.5f, z * freq * 2.5f) * (amplitude * 0.25f) +
                noise(x * freq * 6.0f, z * freq * 6.0f) * (amplitude * 0.1f); // Extra fine detail layer
            heightmap[z * width + x] = h;
        }
    }
}

void Terrain::applyDiagonalSymmetry() {
    // require a square map
    int N = std::min(width, height);
    for (int z = 0; z < N; z++) {
        for (int x = 0; x < z; x++) {
            std::swap(
                heightmap[z * width + x],
                heightmap[x * width + z]
            );
        }
    }
}

// ------------------------------------------------------------
// Compute vertex normals using cross-product of surrounding faces
// ------------------------------------------------------------
void Terrain::computeNormals() {
    vertices.resize(width * height);
    // Fill positions first
    for (int z = 0; z < height; z++) {
        for (int x = 0; x < width; x++) {
            float h = heightmap[z * width + x];
            vertices[z * width + x].pos = glm::vec3(x, h, z);
            vertices[z * width + x].normal = glm::vec3(0, 0, 0); // Start at zero for accumulation
        }
    }
    // Accumulate unnormalized normals from both triangles per quad for better averaging
    for (int z = 0; z < height - 1; z++) {
        for (int x = 0; x < width - 1; x++) {
            vec3 p0 = vertices[z * width + x].pos;
            vec3 p1 = vertices[z * width + x + 1].pos;
            vec3 p2 = vertices[(z + 1) * width + x].pos;
            vec3 p3 = vertices[(z + 1) * width + x + 1].pos;

            // First triangle (p0, p2, p1)
            vec3 n1 = cross(p2 - p0, p1 - p0); // Unnormalized
            vertices[z * width + x].normal += n1;
            vertices[(z + 1) * width + x].normal += n1;
            vertices[z * width + x + 1].normal += n1;

            // Second triangle (p1, p2, p3)
            vec3 n2 = cross(p2 - p1, p3 - p1); // Unnormalized
            vertices[z * width + x + 1].normal += n2;
            vertices[(z + 1) * width + x].normal += n2;
            vertices[(z + 1) * width + x + 1].normal += n2;
        }
    }
    // Normalize after accumulation
    for (auto& v : vertices) {
        v.normal = normalize(v.normal);
    }
}

// ------------------------------------------------------------
// Build VAO/VBO/EBO mesh
// ------------------------------------------------------------
void Terrain::buildMesh() {
    // Create triangle indices
    for (int z = 0; z < height - 1; z++) {
        for (int x = 0; x < width - 1; x++) {
            int i0 = z * width + x;
            int i1 = z * width + x + 1;
            int i2 = (z + 1) * width + x;
            int i3 = (z + 1) * width + x + 1;
            // Triangle 1
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);
            // Triangle 2
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }
    // Upload to OpenGL
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    // Attribute 0 = position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    // Attribute 1 = normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)sizeof(glm::vec3));
    glBindVertexArray(0);
}

void Terrain::draw(const glm::mat4& M) {
    glUniformMatrix4fv(glGetUniformLocation(0, "M"), 1, GL_FALSE, &M[0][0]);
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
}

glm::vec3 Terrain::getHeightAt(float x, float z) const {
    int xi = (int)x;
    int zi = (int)z;
    if (xi < 0 || zi < 0 || xi >= width || zi >= height) return glm::vec3(0);
    return glm::vec3(x, heightmap[zi * width + xi], z);
}