#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <vector>
#include <glm/glm.hpp>
#include <GL/glew.h>
#include "Mesh.h"
#include "Terrain.h" 
#include "Frustum.h"
#include "NavigationGrid.h"

// Simple structure to hold an instance of an object
struct EnvObject {
    Mesh* mesh;           // Pointer to the shared mesh resource
    glm::mat4 modelMatrix; // Pre-computed position/rotation/scale
};

enum class ObstacleType { TREE, ROCK };

struct Obstacle {
    int id;                 // Unique ID to target specific trees
    glm::vec3 position;
    float radius;
    ObstacleType type;
    int resourceAmount;     // How much wood/rock is inside
    bool active;            // True = exists, False = harvested/destroyed
    bool marked = false;

    Mesh* mesh;           // Pointer to the shared mesh resource
    glm::mat4 modelMatrix; // Pre-computed position/rotation/scale
};

class Environment {
public:
    Environment();
    ~Environment();

    // Const getter (for drawing/reading)
    const std::vector<Obstacle>& getObstacles() const { return m_Obstacles; }

    // Non-const getter (for modifying/marking)
    std::vector<Obstacle>& getObstacles() { return m_Obstacles; }

    // Generates the environment.
    // mapSize: Size of your terrain
    // numObjects: How many trees/rocks to spawn inside
    // checks in the navGrid if the placement is valid
    void initialize(Terrain* terrain, NavigationGrid* navGrid, float mapSize, int numObjects);

    // Helper: Find an obstacle under the mouse cursor
    int getObstacleIdAt(glm::vec3 worldPos, float cursorRadius = 2.0f) {
        for (const auto& obs : m_Obstacles) {
            if (!obs.active) continue;
            // Check distance (Mouse Cursor vs Obstacle Center)
            if (glm::distance(worldPos, obs.position) < (obs.radius + cursorRadius)) {
                return obs.id;
            }
        }
        return -1; // Nothing found
    }

    // Helper: Get data by ID
    Obstacle* getObstacleById(int id) {
        for (auto& obs : m_Obstacles) {
            if (obs.id == id) return &obs;
        }
        return nullptr;
    }

    //Setter for textures
    void setTextures(GLuint treeTex, GLuint rockTex) {
        m_treeTexture = treeTex;
        m_rockTexture = rockTex;
    }

    // Draws all environment objects
    // shaderProgram: The shader to use (Standard or Shadow)
    // modelLoc: The uniform location for "M" or "model"
    int draw(GLuint shaderProgram, GLuint modelMatrixLocation, const Frustum& frustum);

    // Frees GPU resources
    void cleanup();

private:
    std::vector<EnvObject> natureObjects;

    std::vector<Obstacle> m_Obstacles;

    // The actual 3D models
    Mesh* treeMesh;
    Mesh* rockMesh;
    Mesh* boulderMesh;

    GLuint m_treeTexture = 0;
    GLuint m_rockTexture = 0;

    // Helper for random generation
    float randomFloat(float min, float max);
};

#endif
