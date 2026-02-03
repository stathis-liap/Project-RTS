#include "Terrain.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "common/texture.h" // ✅ REQUIRED: to use loadSOIL
#include <iostream>

using namespace glm;

Terrain::Terrain(int width, int height, float amplitude)
    : width(width), height(height), amplitude(amplitude),
    vao(0), vbo(0), ebo(0), textureID_(0)
{
    // ✅ 1. LOAD TEXTURE ONCE (Prevents Freezing)
    // Make sure the path matches where your image actually is!
    textureID_ = loadSOIL("models/hexagons_medieval.png");

    if (textureID_ == 0) {
        std::cout << "WARNING: Terrain texture failed to load! Using fallback." << std::endl;
    }

    generate();
}

Terrain::~Terrain()
{
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ebo) glDeleteBuffers(1, &ebo);
}

void Terrain::generate()
{
    generateHeightmap();
    computeNormals();
    buildMesh();
    applyDiagonalSymmetry();
}

float Terrain::hash(int xi, int zi) const
{
    int seed = xi * 49632 + zi * 325176 + 12345;
    seed = (seed << 13) ^ seed;
    return (1.0f - ((seed * (seed * seed * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

float Terrain::noise(float x, float z) const
{
    int xi = static_cast<int>(floor(x));
    int zi = static_cast<int>(floor(z));
    float fx = x - xi;
    float fz = z - zi;

    float ux = fx * fx * (3.0f - 2.0f * fx);
    float uz = fz * fz * (3.0f - 2.0f * fz);

    float n00 = hash(xi, zi);
    float n01 = hash(xi, zi + 1);
    float n10 = hash(xi + 1, zi);
    float n11 = hash(xi + 1, zi + 1);

    float nz0 = mix(n00, n01, uz);
    float nz1 = mix(n10, n11, uz);
    return mix(nz0, nz1, ux);
}

void Terrain::generateHeightmap()
{
    heightmap.resize(width * height);

    float freq = 0.1f;
    for (int z = 0; z < height; ++z)
    {
        for (int x = 0; x < width; ++x)
        {
            float h =
                noise(x * freq, z * freq) * amplitude +
                noise(x * freq * 2.5f, z * freq * 2.5f) * (amplitude * 0.25f) +
                noise(x * freq * 6.0f, z * freq * 6.0f) * (amplitude * 0.1f);

            heightmap[z * width + x] = h;
        }
    }
}

void Terrain::applyDiagonalSymmetry()
{
    int N = std::min(width, height);
    for (int z = 0; z < N; ++z)
    {
        for (int x = 0; x < z; ++x)
        {
            std::swap(heightmap[z * width + x], heightmap[x * width + z]);
        }
    }
}

void Terrain::computeNormals()
{
    vertices.resize(width * height);

    // 1. Set Positions and UVs
    for (int z = 0; z < height; ++z)
    {
        for (int x = 0; x < width; ++x)
        {
            float h = heightmap[z * width + x];
            vertices[z * width + x].pos = vec3(x, h, z);
            vertices[z * width + x].normal = vec3(0.0f);

            // ✅ 2. CALCULATE TEXTURE COORDINATES (UVs)
            // This maps the image across the terrain. 
            // "0.1f" scales the texture so it repeats every 10 units (prevents it looking stretched)
            vertices[z * width + x].texCoords = vec2(x * 0.1f, z * 0.1f);
        }
    }

    // 2. Calculate Normals (Standard Logic)
    for (int z = 0; z < height - 1; ++z)
    {
        for (int x = 0; x < width - 1; ++x)
        {
            vec3 p0 = vertices[z * width + x].pos;
            vec3 p1 = vertices[z * width + x + 1].pos;
            vec3 p2 = vertices[(z + 1) * width + x].pos;
            vec3 p3 = vertices[(z + 1) * width + x + 1].pos;

            vec3 n1 = cross(p2 - p0, p1 - p0);
            vec3 n2 = cross(p2 - p1, p3 - p1);

            vertices[z * width + x].normal += n1;
            vertices[z * width + x + 1].normal += n1 + n2;
            vertices[(z + 1) * width + x].normal += n1 + n2;
            vertices[(z + 1) * width + x + 1].normal += n2;
        }
    }

    // 3. Normalize
    for (auto& v : vertices)
    {
        if (length(v.normal) > 0.0001f)
            v.normal = normalize(v.normal);
        else
            v.normal = vec3(0, 1, 0);
    }
}

void Terrain::buildMesh()
{
    indices.clear();
    for (int z = 0; z < height - 1; ++z)
    {
        for (int x = 0; x < width - 1; ++x)
        {
            unsigned int i0 = z * width + x;
            unsigned int i1 = z * width + x + 1;
            unsigned int i2 = (z + 1) * width + x;
            unsigned int i3 = (z + 1) * width + x + 1;

            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(TerrainVertex), vertices.data(), GL_DYNAMIC_DRAW); // ✅ Changed to DYNAMIC_DRAW

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, pos));

    // normal attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, normal));

    // ✅ 3. TEXTURE COORDINATES (Attribute 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, texCoords));

    glBindVertexArray(0);
}

void Terrain::draw(const glm::mat4& model)
{
    GLuint currentProgram;
    glGetIntegerv(GL_CURRENT_PROGRAM, (GLint*)&currentProgram);

    GLuint modelLoc = glGetUniformLocation(currentProgram, "M");
    if (modelLoc != GL_INVALID_INDEX)
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void Terrain::draw(const glm::mat4& model, GLuint shaderProgram)
{
    glUseProgram(shaderProgram);

    GLuint modelLoc = glGetUniformLocation(shaderProgram, "model");
    if (modelLoc != GL_INVALID_INDEX)
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);

    // ✅ BIND TEXTURE
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID_);

    // Tell shader to use unit 0 (Optional safety, usually done in main loop)
    glUniform1i(glGetUniformLocation(shaderProgram, "diffuseColorSampler"), 0);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

glm::vec3 Terrain::getHeightAt(float x, float z) const
{
    int xi = static_cast<int>(x);
    int zi = static_cast<int>(z);

    if (xi < 0 || zi < 0 || xi >= width - 1 || zi >= height - 1)
        return vec3(x, 0.0f, z);

    float h = heightmap[zi * width + xi];
    return vec3(x, h, z);
}

// ============================================================
// ✅ NEW: Flatten terrain area for building placement
// ============================================================
void Terrain::flattenArea(const glm::vec3& center, float radius)
{
    // Convert world position to grid coordinates
    int centerX = static_cast<int>(center.x);
    int centerZ = static_cast<int>(center.z);

    // Get the height at the center point
    float targetHeight = heightmap[centerZ * width + centerX];

    // Flatten in a circle around the center
    int radiusInt = static_cast<int>(radius) + 1;

    for (int z = centerZ - radiusInt; z <= centerZ + radiusInt; ++z)
    {
        for (int x = centerX - radiusInt; x <= centerX + radiusInt; ++x)
        {
            // Check bounds
            if (x < 0 || x >= width || z < 0 || z >= height) continue;

            // Check if within circular radius
            float dx = x - center.x;
            float dz = z - center.z;
            float distance = sqrt(dx * dx + dz * dz);

            if (distance <= radius)
            {
                // Smooth falloff at edges (quadratic)
                float falloff = 1.0f - (distance / radius);
                falloff = falloff * falloff; // Square for smoother transition

                // Blend between current height and target height
                int index = z * width + x;
                float currentHeight = heightmap[index];
                heightmap[index] = glm::mix(currentHeight, targetHeight, falloff);

                // Update vertex position
                vertices[index].pos.y = heightmap[index];
            }
        }
    }

    // ✅ Recalculate normals for affected area
    recalculateNormalsInArea(centerX, centerZ, radiusInt);

    // ✅ Update GPU buffer with new vertex data
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(TerrainVertex), vertices.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ============================================================
// ✅ NEW: Recalculate normals for a specific area
// ============================================================
void Terrain::recalculateNormalsInArea(int centerX, int centerZ, int radius)
{
    // Expand radius slightly to ensure smooth transitions
    int expandedRadius = radius + 2;

    int minX = std::max(0, centerX - expandedRadius);
    int maxX = std::min(width - 1, centerX + expandedRadius);
    int minZ = std::max(0, centerZ - expandedRadius);
    int maxZ = std::min(height - 1, centerZ + expandedRadius);

    // Reset normals in affected area
    for (int z = minZ; z <= maxZ; ++z)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            vertices[z * width + x].normal = vec3(0.0f);
        }
    }

    // Recalculate normals from surrounding triangles
    for (int z = minZ; z < maxZ; ++z)
    {
        for (int x = minX; x < maxX; ++x)
        {
            vec3 p0 = vertices[z * width + x].pos;
            vec3 p1 = vertices[z * width + x + 1].pos;
            vec3 p2 = vertices[(z + 1) * width + x].pos;
            vec3 p3 = vertices[(z + 1) * width + x + 1].pos;

            vec3 n1 = cross(p2 - p0, p1 - p0);
            vec3 n2 = cross(p2 - p1, p3 - p1);

            vertices[z * width + x].normal += n1;
            vertices[z * width + x + 1].normal += n1 + n2;
            vertices[(z + 1) * width + x].normal += n1 + n2;
            vertices[(z + 1) * width + x + 1].normal += n2;
        }
    }

    // Normalize all normals in affected area
    for (int z = minZ; z <= maxZ; ++z)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            vec3& n = vertices[z * width + x].normal;
            if (length(n) > 0.0001f)
                n = normalize(n);
            else
                n = vec3(0, 1, 0);
        }
    }
}