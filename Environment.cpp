#include "Environment.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cstdlib> 
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

void Environment::initialize(Terrain* terrain, NavigationGrid* navGrid, float mapSize, int numObjects) {
    m_Obstacles.clear();
    natureObjects.clear();

    // Load the models
    if (!treeMesh) treeMesh = new Mesh("models/trees_B_small.obj");
    if (!rockMesh) rockMesh = new Mesh("models/rock_single_D.obj");
    if (!boulderMesh) boulderMesh = new Mesh("models/mountain_A.obj");

    float edgeMargin = 30.0f;
    int obstacleIDCounter = 0;

    // adding Border Rocks
    float step = 15.0f;
    for (float x = 0; x <= mapSize; x += step) {
        for (float z = 0; z <= mapSize; z += step) {
            bool isEdge = (x < edgeMargin || x > mapSize - edgeMargin || z < edgeMargin || z > mapSize - edgeMargin);
            if (isEdge) {
                float y = terrain ? terrain->getHeightAt(x, z).y : 0.0f;
                glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y - 1.0f, z));
                float scale = randomFloat(10.0f, 20.0f);
                model = glm::scale(model, glm::vec3(scale));

                natureObjects.push_back({ boulderMesh, model });

               //add to the grid, preventing objects from being placed inside the border rocks
                if (navGrid) navGrid->updateArea(glm::vec3(x, 0, z), scale, true);
            }
        }
    }

    // inner objects placement (Interior Objects)
    for (int i = 0; i < numObjects; ++i) {
        float x = randomFloat(edgeMargin, mapSize - edgeMargin);
        float z = randomFloat(edgeMargin, mapSize - edgeMargin);

        // collision detection, if pos not available -> skip
        if (navGrid && navGrid->isBlocked((int)x, (int)z)) {
            continue;
        }

        // check distances from Town Halls
        glm::vec2 playerBase(50.0f, 50.0f);
        glm::vec2 enemyBase(mapSize - 50.0f, mapSize - 50.0f);
        if (glm::distance(glm::vec2(x, z), playerBase) < 80.0f ||
            glm::distance(glm::vec2(x, z), enemyBase) < 80.0f) continue;

        float y = terrain ? terrain->getHeightAt(x, z).y : 0.0f;
        float radius = (randomFloat(0, 1) > 0.3f) ? 5.0f : 4.0f; // tree or rock radius

        // Create the matrices
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
        model = glm::rotate(model, glm::radians(randomFloat(0, 360)), glm::vec3(0, 1, 0));

        if (radius == 5.0f) { // Tree
            model = glm::scale(model, glm::vec3(randomFloat(6.0f, 10.0f)));
            m_Obstacles.push_back({ obstacleIDCounter++, glm::vec3(x, y, z), radius, ObstacleType::TREE, 100, true, false, treeMesh, model });
        }
        else { // Rock
            model = glm::scale(model, glm::vec3(randomFloat(20.0f, 22.0f)));
            m_Obstacles.push_back({ obstacleIDCounter++, glm::vec3(x, y, z), radius, ObstacleType::ROCK, 100, true, false, rockMesh, model });
        }

        // add to the grid, make it known that this position is not available
        if (navGrid) navGrid->updateArea(glm::vec3(x, 0, z), radius + 0.5f, true);
        // +0.5 paddind so they dont touch
    }
}

int Environment::draw(GLuint shaderProgram, GLuint modelMatrixLocation, const Frustum& frustum) {
    int drawnCount = 0;

    // Ensure we are operating on Texture Unit 0 (Standard Shader expects this)
    glActiveTexture(GL_TEXTURE0);


    // Draw Decorative Objects (Border Rocks)
    // Optimization: Bind the Rock texture once before the loop.
    if (m_rockTexture != 0) {
        glBindTexture(GL_TEXTURE_2D, m_rockTexture);
    }

    for (const auto& obj : natureObjects) {
        if (!obj.mesh) continue;

        glUniformMatrix4fv(modelMatrixLocation, 1, GL_FALSE, &obj.modelMatrix[0][0]);
        obj.mesh->draw();
        drawnCount++;
    }

    
    // Draw Interactive Objects (Trees & Rocks)
    // Since the vector mixes Trees and Rocks, we check the type and switch textures.
    // Keep track of currently bound texture to avoid useless API calls
    GLuint currentTexture = m_rockTexture;

    for (const auto& obs : m_Obstacles) {
        // Skip dead or invalid objects
        if (!obs.active || !obs.mesh) continue;

        // Frustum Culling
        if (frustum.isSphereVisible(obs.position, obs.radius + 1.0f)) {

            // TEXTURE SWITCHING LOGIC
            if (obs.type == ObstacleType::TREE) {
                if (currentTexture != m_treeTexture) {
                    glBindTexture(GL_TEXTURE_2D, m_treeTexture);
                    currentTexture = m_treeTexture;
                }
            }
            else if (obs.type == ObstacleType::ROCK) {
                if (currentTexture != m_rockTexture) {
                    glBindTexture(GL_TEXTURE_2D, m_rockTexture);
                    currentTexture = m_rockTexture;
                }
            }

            // Draw Mesh
            glUniformMatrix4fv(modelMatrixLocation, 1, GL_FALSE, &obs.modelMatrix[0][0]);
            obs.mesh->draw();
            drawnCount++;
        }
    }

    return drawnCount; //Return the total visible objects
}