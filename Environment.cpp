#include "Environment.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cstdlib> // for rand()
#include <iostream>

Environment::Environment()
    : treeMesh(nullptr), rockMesh(nullptr), boulderMesh(nullptr)
{
}

Environment::~Environment() {
    cleanup();
}

void Environment::cleanup() {
    if (treeMesh) { delete treeMesh; treeMesh = nullptr; }
    if (rockMesh) { delete rockMesh; rockMesh = nullptr; }
    if (boulderMesh) { delete boulderMesh; boulderMesh = nullptr; }
    natureObjects.clear();
}

float Environment::randomFloat(float min, float max) {
    return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
}

void Environment::initialize(Terrain* terrain, float mapSize, int numObjects) {
    m_Obstacles.clear();
    natureObjects.clear();

    // Load Models
    if (!treeMesh) treeMesh = new Mesh("models/trees_B_small.obj");
    if (!rockMesh) rockMesh = new Mesh("models/rock_single_D.obj");
    if (!boulderMesh) boulderMesh = new Mesh("models/mountain_A.obj");

    float edgeMargin = 30.0f;
    int obstacleIDCounter = 0;

    // 1. BORDER ROCKS (Decorative only - stored in natureObjects)
    float step = 15.0f;
    for (float x = 0; x <= mapSize; x += step) {
        for (float z = 0; z <= mapSize; z += step) {
            bool isEdge = (x < edgeMargin || x > mapSize - edgeMargin || z < edgeMargin || z > mapSize - edgeMargin);
            if (isEdge) {
                float y = terrain ? terrain->getHeightAt(x, z).y : 0.0f;
                y -= 1.0f;
                glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
                model = glm::scale(model, glm::vec3(randomFloat(10.0f, 20.0f)));

                // Add to decorative list (always drawn)
                if (boulderMesh) natureObjects.push_back({ boulderMesh, model });
            }
        }
    }

    // 2. INTERIOR OBJECTS (Harvestable - stored in m_Obstacles)
    for (int i = 0; i < numObjects; ++i) {
        float x = randomFloat(edgeMargin, mapSize - edgeMargin);
        float z = randomFloat(edgeMargin, mapSize - edgeMargin);

        // ✅ FIX: Define BOTH base locations
        glm::vec2 playerBase(50.0f, 50.0f);
        glm::vec2 enemyBase(mapSize - 50.0f, mapSize - 50.0f); // Opposite corner
        float safeRadius = 80.0f;

        // ✅ FIX: Check distance to BOTH bases
        if (glm::distance(glm::vec2(x, z), playerBase) < safeRadius ||
            glm::distance(glm::vec2(x, z), enemyBase) < safeRadius) {
            continue; // Skip spawn if too close to either town center
        }

        float y = terrain ? terrain->getHeightAt(x, z).y : 0.0f;
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
        model = glm::rotate(model, glm::radians(randomFloat(0, 360)), glm::vec3(0, 1, 0));

        // TREE
        if (randomFloat(0, 1) > 0.3f) {
            float scale = randomFloat(6.0f, 10.0f);
            model = glm::scale(model, glm::vec3(scale));

            // ✅ Store EVERYTHING in m_Obstacles
            m_Obstacles.push_back({
                obstacleIDCounter++,
                glm::vec3(x, y, z),
                2.0f,
                ObstacleType::TREE,
                100,
                true,   // Active
                false,  // Marked
                treeMesh, // Visual Mesh
                model     // Visual Matrix
                });
        }
        // ROCK
        else {
            float scale = randomFloat(20.0f, 22.0f);
            model = glm::scale(model, glm::vec3(scale));

            m_Obstacles.push_back({
                obstacleIDCounter++,
                glm::vec3(x, y, z),
                3.0f,
                ObstacleType::ROCK,
                100,
                true,
                false,
                rockMesh,
                model
                });
        }
    }
}

void Environment::draw(GLuint shaderProgram, GLuint modelMatrixLocation) {
    // 1. Draw Decorative Objects (Border Rocks)
    for (const auto& obj : natureObjects) {
        if (!obj.mesh) continue;
        glUniformMatrix4fv(modelMatrixLocation, 1, GL_FALSE, &obj.modelMatrix[0][0]);
        obj.mesh->draw();
    }

    // 2. Draw Interactive Objects (Trees/Rocks)
    // ✅ ONLY DRAW IF ACTIVE
    for (const auto& obs : m_Obstacles) {
        if (obs.active && obs.mesh) {
            glUniformMatrix4fv(modelMatrixLocation, 1, GL_FALSE, &obs.modelMatrix[0][0]);
            obs.mesh->draw();
        }
    }
}