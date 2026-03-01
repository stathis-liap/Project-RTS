#include <iostream>
#include <string>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GL/glew.h>
#include <glfw3.h>
#include <common/shader.h>
#include <common/util.h>
#include <common/camera.h>
#include "common/texture.h"

#include <glm/gtc/type_ptr.hpp> 

#include "Terrain.h"
#include "ShadowMap.h"
#include "SnowTrailMap.h"
#include "Building.h"
#include <vector>
#include <memory>
#include "Unit.h"
#include "Environment.h"
#include "Resource.h"
#include "SkinnedMesh.h"
#include "Pathfinder.h"
#include "NavigationGrid.h"
#include "Frustum.h"

#include "ParticleManager.h"

// ---------------------------------------------------------------
using namespace std;
using namespace glm;
// ---------------------------------------------------------------
#define W_WIDTH 1920 //1920
#define W_HEIGHT 1080 //1080
#define TITLE "RTS Project"
// ---------------------------------------------------------------
GLFWwindow* window = nullptr;
Camera* camera = nullptr;
GLuint shaderProgram = 0;
GLuint terrainShader = 0;
SnowTrailMap* snowTrailMap = nullptr;
GLuint snowTrailShader = 0;
ShadowMap* shadowMap = nullptr;
GLuint shadowShader = 0;
GLuint debugShader = 0;
GLuint quadVAO = 0, quadVBO = 0, quadEBO = 0;
GLuint dummyPointVAO = 0, dummyPointVBO = 0;
GLuint shadowMapTexUnit = 5;
bool showDepthMap = false;
GLuint shadowMapLoc_standard = 0;
GLuint lightSpaceMatrixLoc_standard = 0;
GLuint instancedShader = 0;

GLuint particleShaderProgram = 0;

std::vector<std::unique_ptr<IntParticleEmitter>> ParticleManager::active_emitters;
Drawable* ParticleManager::particle_quad = nullptr;

Frustum cameraFrustum;

Environment* environment = nullptr;

SkinnedMesh* myActor = nullptr;
//GLuint skinnedShader = 0;

GLuint texWorker = 0;
GLuint texWarrior = 0;
GLuint texMage = 0;

// Building Textures
GLuint texTownCenter = 0;
GLuint texBarracks = 0;
GLuint texArchery = 0;

// Nature Textures (pass these to Environment later)
GLuint texTree = 0;
GLuint texRock = 0;

GLuint fireTexture = 0;

GLuint whiteShader = 0;

NavigationGrid* navGrid = nullptr;


// Uniform locations (standard shader)
GLuint projectionMatrixLocation, viewMatrixLocation, modelMatrixLocation;
GLuint KaLocation, KdLocation, KsLocation, NsLocation;
GLuint LaLocation, LdLocation, LsLocation, lightDirectionLocation, lightPowerLocation;

// ---------------------------------------------------------------
// SELECTION & INPUT GLOBALS
// ---------------------------------------------------------------
enum class InputMode { UNIT_SELECT, RESOURCE_SELECT, ATTACK_SELECT};
InputMode currentMode = InputMode::UNIT_SELECT; // Default

// Left Click (Generic Dragging)
bool isDragging = false;
bool isRightDragging = false;
glm::vec3 dragStartWorld;
glm::vec3 dragEndWorld;

// Camera Mode State
bool unitCameraMode = false;
Unit* focusedUnit = nullptr;

// ---------------------------------------------------------------
struct Light {
    vec4 La, Ld, Ls;
    vec3 dir;
    float power;
};
struct Material {
    vec4 Ka, Kd, Ks;
    float Ns;
};
const Material defaultMaterial{
    vec4{0.1f,0.1f,0.1f,1},
    vec4{1.0f,1.0f,1.0f,1.0f}, 
    vec4{0.3f,0.3f,0.3f,1},
    20.0f
};
Light light{
    vec4{0.2f,0.2f,0.2f,1},
    vec4{1,1,1,1},
    vec4{1,1,1,1},
    vec3(0,10,0),
    1.0f
};
Terrain* terrain = nullptr;
std::vector<std::unique_ptr<Building>> buildings;
bool placingBuilding = false;
bool isPlacementValid = false;
BuildingType currentPlaceType = BuildingType::TOWN_CENTER;
std::unique_ptr<Building> previewBuilding = nullptr;
std::vector<std::unique_ptr<Unit>> units;

Resources playerResources;
// ---------------------------------------------------------------

void createContext()
{
    // Standard shader

    shaderProgram = loadShaders("StandardShading.vertexshader", "StandardShading.fragmentshader");
    GLint isLinked = 0;

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &isLinked);

    if (isLinked == GL_FALSE) {
        GLint maxLength = 0;
        glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &maxLength);
        std::vector<char> infoLog(maxLength);
        glGetProgramInfoLog(shaderProgram, maxLength, &maxLength, &infoLog[0]);

        std::cout << "CRITICAL SHADER ERROR: " << &infoLog[0] << std::endl;

        return; // Stop execution here so you can read the error

    }

    projectionMatrixLocation = glGetUniformLocation(shaderProgram, "P");

    viewMatrixLocation = glGetUniformLocation(shaderProgram, "V");

    modelMatrixLocation = glGetUniformLocation(shaderProgram, "M");

    KaLocation = glGetUniformLocation(shaderProgram, "mtl.Ka");

    KdLocation = glGetUniformLocation(shaderProgram, "mtl.Kd");

    KsLocation = glGetUniformLocation(shaderProgram, "mtl.Ks");

    NsLocation = glGetUniformLocation(shaderProgram, "mtl.Ns");

    LaLocation = glGetUniformLocation(shaderProgram, "light.La");

    LdLocation = glGetUniformLocation(shaderProgram, "light.Ld");

    LsLocation = glGetUniformLocation(shaderProgram, "light.Ls");

    lightDirectionLocation = glGetUniformLocation(shaderProgram, "lightDirection_worldspace");

    lightPowerLocation = glGetUniformLocation(shaderProgram, "light.power");

    shadowMapLoc_standard = glGetUniformLocation(shaderProgram, "shadowMap");

    lightSpaceMatrixLoc_standard = glGetUniformLocation(shaderProgram, "lightSpaceMatrix");

    glUseProgram(shaderProgram);

    glUniform1i(shadowMapLoc_standard, 5);

    glUseProgram(0);


    // Terrain shader

    terrainShader = loadShaders("terrain.vertexshader", "terrain.fragmentshader");
    terrain = new Terrain(512, 512, 1.05f);


    // Create environment
    environment = new Environment();
    //create navigation grid
    navGrid = new NavigationGrid(512, 512);

    environment->initialize(terrain,navGrid, 512.0f, 1500);
    environment->setTextures(texTree, texRock);

    shadowShader = loadShaders("ShadowMap.vertexshader", "ShadowMap.fragmentshader");

    shadowMap = new ShadowMap(4096, 4096);
    snowTrailMap = new SnowTrailMap(2048, 2048);

    snowTrailMap->clear(); // FULL SNOW at start
    snowTrailShader = loadShaders("SnowTrail.vertexshader", "SnowTrail.fragmentshader");
    debugShader = loadShaders("debugDepthShader.vertexshader", "debugDepthShader.fragmentshader");
    whiteShader = loadShaders("White.vertexshader", "White.fragmentshader");
    //skinnedShader = loadShaders("SkinnedShading.vertexshader", "SkinnedShading.fragmentshader");
    instancedShader = loadShaders("SkinnedInstanced.vertexshader", "SkinnedInstanced.fragmentshader");

    // Debug quad
    float quadVertices[] = {

        -1.0f, -1.0f, 0.0f, 0.0f,

         1.0f, -1.0f, 1.0f, 0.0f,

         1.0f, 1.0f, 1.0f, 1.0f,

        -1.0f, 1.0f, 0.0f, 1.0f

    };

    unsigned int indices[] = { 0, 1, 2, 0, 2, 3 };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glGenBuffers(1, &quadEBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    // DUMMY VAO/VBO for snow trail points

    glGenVertexArrays(1, &dummyPointVAO);
    glGenBuffers(1, &dummyPointVBO);
    glBindVertexArray(dummyPointVAO);
    glBindBuffer(GL_ARRAY_BUFFER, dummyPointVBO);

    float dummy = 0.0f;

    glBufferData(GL_ARRAY_BUFFER, sizeof(float), &dummy, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0);
    glBindVertexArray(0);


    // Starting content
    vec3 myTownCenterPos = vec3(50.0f, 0.0f, 50.0f);
    terrain->flattenArea(myTownCenterPos, 15.0f);  // Flatten first
    buildings.push_back(std::make_unique<Building>(BuildingType::TOWN_CENTER, myTownCenterPos,0));
    buildings.back()->updateConstruction(10000.0f);


    vec3 enemyTownCenterPos = vec3(460.0f, 0.0f, 460.0f);
    terrain->flattenArea(enemyTownCenterPos, 15.0f);  //Flatten first
    buildings.push_back(std::make_unique<Building>(BuildingType::TOWN_CENTER, enemyTownCenterPos,1));
    buildings.back()->updateConstruction(10000.0f);

    vec3 enemyBarracksCenterPos = vec3(50.0f, 0.0f, 120.0f);
    terrain->flattenArea(enemyBarracksCenterPos, 15.0f);  //Flatten first
    buildings.push_back(std::make_unique<Building>(BuildingType::BARRACKS, enemyBarracksCenterPos, 1));
    buildings.back()->updateConstruction(10000.0f);

    // ---------------------------------------------------------
    // PERFORMANCE TEST: MASSIVE ARMY SPAWNER
    // ---------------------------------------------------------

    // Army 1: Player (Green/Team 0)
    //int rows = 10;
    //int cols = 10; // 10 * 10 = 100 Units
    //float spacing = 2.5f;
    //vec3 startPosPlayer(220.0f, 0.0f, 220.0f);

    //for (int i = 0; i < rows; ++i) {
    //    for (int j = 0; j < cols; ++j) {
    //        float x = startPosPlayer.x + (j * spacing);
    //        float z = startPosPlayer.z + (i * spacing);

    //        // Mix up unit types for variety
    //        UnitType type;
    //        int r = (i + j) % 3;
    //        if (r == 0) type = UnitType::MELEE;
    //        else if (r == 1) type = UnitType::RANGED;
    //        else type = UnitType::WORKER;

    //        // Optional: Check grid to avoid spawning inside rocks
    //        if (navGrid && !navGrid->isBlocked((int)x, (int)z)) {
    //            units.push_back(std::make_unique<Unit>(type, vec3(x, 0.0f, z), 0));
    //        }
    //    }
    //}

    //// Army 2: Enemy (Red/Team 1)
    //vec3 startPosEnemy(300.0f, 0.0f, 300.0f);

    //for (int i = 0; i < rows; ++i) {
    //    for (int j = 0; j < cols; ++j) {
    //        float x = startPosEnemy.x + (j * spacing);
    //        float z = startPosEnemy.z + (i * spacing);

    //        UnitType type;
    //        int r = (i + j) % 3;
    //        if (r == 0) type = UnitType::MELEE;
    //        else if (r == 1) type = UnitType::RANGED;
    //        else type = UnitType::WORKER;

    //        if (navGrid && !navGrid->isBlocked((int)x, (int)z)) {
    //            units.push_back(std::make_unique<Unit>(type, vec3(x, 0.0f, z), 1));
    //        }
    //    }
    //}

    //std::cout << "--- PERFORMANCE TEST STARTED ---" << std::endl;
    //std::cout << "Total Units Spawned: " << units.size() << std::endl;
   

// BAKE STATIC OBSTACLES (Trees/Rocks)
    const std::vector<Obstacle>& envObstacles = environment->getObstacles();
    for (const auto& obs : envObstacles) {
        navGrid->updateArea(obs.position, obs.radius, true);
    }

    // BAKE BUILDINGS
    for (const auto& b : buildings) {
        float r = (b->getType() == BuildingType::TOWN_CENTER) ? 12.0f : 8.0f;
        navGrid->updateArea(b->getPosition(), r, true);
    }

    //  BAKE MAP BORDERS 
    // This ensures units never walk into the edge rocks or off the map
    int mapSize = 512;
    int borderThickness = 30; // Width of the rock border

    // We loop through the grid pixels
    for (int x = 0; x < mapSize; x++) {
        for (int z = 0; z < mapSize; z++) {

            // If we are close to ANY edge (Left, Right, Top, Bottom)
            if (x < borderThickness || x >= mapSize - borderThickness ||
                z < borderThickness || z >= mapSize - borderThickness) {

                // Mark as BLOCKED manually
                navGrid->updateArea(glm::vec3(x, 0, z), 0.5f, true);
            }
        }
    }

    //Particles
    ParticleManager::init(new Drawable("models/sphere.obj"));
    particleShaderProgram = loadShaders("ParticleShader.vertexshader", "ParticleShader.fragmentshader");
}

void checkGLError(const std::string& label) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cout << "GL Error at " << label << ": " << std::hex << err << std::endl;
    }
}

void uploadMaterial(const Material& m) {
    glUniform4f(KaLocation, m.Ka.r, m.Ka.g, m.Ka.b, m.Ka.a);
    glUniform4f(KdLocation, m.Kd.r, m.Kd.g, m.Kd.b, m.Kd.a);
    glUniform4f(KsLocation, m.Ks.r, m.Ks.g, m.Ks.b, m.Ks.a);
    glUniform1f(NsLocation, m.Ns);
}
void uploadLight(const Light& l) {
    glUniform4f(LaLocation, l.La.r, l.La.g, l.La.b, l.La.a);
    glUniform4f(LdLocation, l.Ld.r, l.Ld.g, l.Ld.b, l.Ld.a);
    glUniform4f(LsLocation, l.Ls.r, l.Ls.g, l.Ls.b, l.Ls.a);
    glUniform3f(lightDirectionLocation, l.dir.x, l.dir.y, l.dir.z);
    glUniform1f(lightPowerLocation, l.power);
}

// Get world position under cursor
vec3 getWorldPosUnderCursor()
{
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    my = W_HEIGHT - my;
    mat4 proj = camera->projectionMatrix;
    mat4 view = camera->viewMatrix;
    vec4 viewport(0, 0, W_WIDTH, W_HEIGHT);
    vec3 nearPoint = unProject(vec3((float)mx, (float)my, 0.0f), view, proj, viewport);
    vec3 farPoint = unProject(vec3((float)mx, (float)my, 1.0f), view, proj, viewport);
    vec3 dir = normalize(farPoint - nearPoint);
    if (abs(dir.y) < 0.0001f) return vec3(0.0f);
    float t = -nearPoint.y / dir.y;
    if (t < 0) return vec3(0.0f);
    return nearPoint + dir * t;
}

// Helper: Convert World 3D to Screen 2D
vec2 worldToScreen(vec3 worldPos, mat4 view, mat4 proj) {
    vec4 clip = proj * view * vec4(worldPos, 1.0f);
    if (clip.w <= 0) return vec2(-1, -1); // Behind camera

    vec3 ndc = vec3(clip) / clip.w;
    float screenX = (ndc.x + 1.0f) * 0.5f * W_WIDTH;
    float screenY = (1.0f - ndc.y) * 0.5f * W_HEIGHT; // Flip Y for OpenGL coords
    return vec2(screenX, screenY);
}

// Logic: Handle ALL Mouse Input (Left & Right)
void handleInput() {
    if (placingBuilding) return;

    // -------------------------------------------------------
    // 1. TOGGLE MODES
    // -------------------------------------------------------
    static bool lastR = false;
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS && !lastR) {
        currentMode = (currentMode == InputMode::RESOURCE_SELECT) ? InputMode::UNIT_SELECT : InputMode::RESOURCE_SELECT;
        std::cout << ">>> MODE: " << (currentMode == InputMode::RESOURCE_SELECT ? "RESOURCE" : "UNIT") << " <<<" << std::endl;
    }
    lastR = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;

    static bool lastB = false;
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS && !lastB) {
        currentMode = (currentMode == InputMode::ATTACK_SELECT) ? InputMode::UNIT_SELECT : InputMode::ATTACK_SELECT;
        std::cout << ">>> MODE: " << (currentMode == InputMode::ATTACK_SELECT ? "ATTACK" : "UNIT") << " <<<" << std::endl;
    }
    lastB = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;

    // -------------------------------------------------------
    // 2. LEFT MOUSE: SELECTION & ACTION
    // -------------------------------------------------------
    static bool wasLeftPressed = false;
    bool isLeftPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    if (isLeftPressed && !wasLeftPressed) {
        isDragging = true;
        dragStartWorld = getWorldPosUnderCursor();
    }
    if (isDragging) dragEndWorld = getWorldPosUnderCursor();

    if (!isLeftPressed && wasLeftPressed && isDragging) {
        isDragging = false;
        mat4 P = camera->projectionMatrix;
        mat4 V = camera->viewMatrix;
        vec2 startScreen = worldToScreen(dragStartWorld, V, P);
        vec2 endScreen = worldToScreen(dragEndWorld, V, P);

        float minX = std::min(startScreen.x, endScreen.x);
        float maxX = std::max(startScreen.x, endScreen.x);
        float minY = std::min(startScreen.y, endScreen.y);
        float maxY = std::max(startScreen.y, endScreen.y);
        bool isClick = (maxX - minX < 5 && maxY - minY < 5);
        bool shiftHeld = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;

        // =========================================================
        // MODE A: STANDARD UNIT SELECTION (Green Box)
        // =========================================================
        if (currentMode == InputMode::UNIT_SELECT) {
            if (!shiftHeld) {
                for (auto& u : units) u->setSelected(false);
                focusedUnit = nullptr;
                unitCameraMode = false;
            }
            if (isClick) {
                float minDist = 5.0f;
                Unit* closest = nullptr;
                for (auto& u : units) {
                    float dist = distance(u->getPosition(), dragEndWorld);
                    if (dist < minDist) { minDist = dist; closest = u.get(); }
                }
                if (closest) closest->setSelected(true);
            }
            else {
                for (auto& u : units) {
                    vec2 sPos = worldToScreen(u->getPosition(), V, P);
                    if (sPos.x >= minX && sPos.x <= maxX && sPos.y >= minY && sPos.y <= maxY) {
                        u->setSelected(true);
                    }
                }
            }
        }
        // =========================================================
        // MODE B: RESOURCE TARGETING (Yellow Box)
        // =========================================================
        else if (currentMode == InputMode::RESOURCE_SELECT) {
            std::vector<Unit*> workers;
            for (auto& u : units) {
                if (u->isSelected() && u->getType() == UnitType::WORKER) {
                    workers.push_back(u.get());
                }
            }

            if (!workers.empty()) {
                auto& allObs = environment->getObstacles();

                // 1. Collect ALL selected resources first
                std::vector<int> targetResources;

                for (auto& obs : allObs) {
                    if (!obs.active) continue;

                    bool inBox = false;
                    if (isClick) {
                        // Click selection (Radius check)
                        if (glm::distance(obs.position, dragEndWorld) < obs.radius + 1.0f) inBox = true;
                    }
                    else {
                        // Box selection (Screen space check)
                        glm::vec2 sPos = worldToScreen(obs.position, V, P);
                        if (sPos.x >= minX && sPos.x <= maxX && sPos.y >= minY && sPos.y <= maxY) inBox = true;
                    }

                    if (inBox) {
                        obs.marked = true; // Visual feedback
                        targetResources.push_back(obs.id); // Add ID to list
                    }
                }

                // 2. Assign the list to workers (They will shuffle it internally)
                if (!targetResources.empty()) {
                    for (auto* w : workers) {
                        w->assignGatherQueue(targetResources);
                    }
                    std::cout << "Assigned " << targetResources.size() << " resources to " << workers.size() << " workers." << std::endl;
                }
            }

            // Auto-switch back to normal mode
            currentMode = InputMode::UNIT_SELECT;
        }
        // =========================================================
        // MODE C: ATTACK TARGETING (Red Box)
        // =========================================================
        else if (currentMode == InputMode::ATTACK_SELECT) {
            std::vector<Unit*> myUnits;
            for (auto& u : units) if (u->isSelected() && u->getTeam() == 0) myUnits.push_back(u.get());

            if (!myUnits.empty()) {
                vec3 clickPos = dragEndWorld;
                bool commandIssued = false;

                // ---------------------------------------------
                // 1. Check for Enemy UNITS
                // ---------------------------------------------
                std::vector<Unit*> enemyTargets;
                for (auto& u : units) {
                    if (u->getTeam() == 1) { // Is Enemy
                        bool targeted = false;
                        if (isClick) {
                            if (distance(u->getPosition(), clickPos) < 10.0f) targeted = true;
                        }
                        else {
                            vec2 sPos = worldToScreen(u->getPosition(), V, P);
                            if (sPos.x >= minX && sPos.x <= maxX && sPos.y >= minY && sPos.y <= maxY) targeted = true;
                        }
                        if (targeted) enemyTargets.push_back(u.get());
                    }
                }

                if (!enemyTargets.empty()) {
                    //std::cout << "Command: ATTACK UNITS (" << enemyTargets.size() << " enemies queued)" << std::endl;
                    commandIssued = true;
                    // SMART QUEUEING (Units)
                    for (auto* myUnit : myUnits) {
                        std::vector<Unit*> sortedTargets = enemyTargets;
                        std::sort(sortedTargets.begin(), sortedTargets.end(),
                            [myUnit](Unit* a, Unit* b) {
                                float distA = glm::distance(myUnit->getPosition(), a->getPosition());
                                float distB = glm::distance(myUnit->getPosition(), b->getPosition());
                                return distA < distB;
                            });
                        myUnit->assignAttackQueue(sortedTargets);
                    }
                }

                // ---------------------------------------------
                // 2. Check for Enemy BUILDINGS (If no units clicked)
                // ---------------------------------------------
                if (!commandIssued && isClick) {
                    for (const auto& b : buildings) {
                        // Check if Enemy (Team 1)
                        if (b->getTeam() == 1) {
                            // Hitbox check (Radius approx 15-20)
                            float dist = glm::distance(clickPos, b->getPosition());
                            if (dist < 20.0f) {
                                std::cout << "Command: ATTACK BUILDING!" << std::endl;
                                commandIssued = true;

                                // Order all selected units to attack this building
                                for (auto* myUnit : myUnits) {
                                    myUnit->assignAttackTask(b.get());
                                }
                                break; // Target found
                            }
                        }
                    }
                }

                if (!commandIssued) {
                    std::cout << "Attack command cancelled (No targets clicked)" << std::endl;
                }
            }
            // Reset cursor to normal after command
            currentMode = InputMode::UNIT_SELECT;
        }
    }
    wasLeftPressed = isLeftPressed;

    // -------------------------------------------------------
    // 3. RIGHT MOUSE: MOVE (Always available as fallback)
    // -------------------------------------------------------
    static bool wasRightPressed = false;
    bool isRightPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    if (isRightPressed && !wasRightPressed) {
        isRightDragging = true;
        dragStartWorld = getWorldPosUnderCursor(); // Reuse global for start pos
    }
    // We use the same globals for drag start/end to simplify visuals, 
    // but logic handles them separately below.
    if (isRightDragging) dragEndWorld = getWorldPosUnderCursor();

    if (!isRightPressed && wasRightPressed && isRightDragging) {
        isRightDragging = false;
        vec3 clickPos = dragEndWorld;

        // Collect My Selected Units
        std::vector<Unit*> myUnits;
        for (auto& u : units) if (u->isSelected() && u->getTeam() == 0) myUnits.push_back(u.get());

        if (!myUnits.empty()) {
            std::cout << "Command: Move (Shared Path + Formation)" << std::endl;

            // 1. Calculate Group Center
            vec3 groupCenter(0.0f);
            for (auto* u : myUnits) groupCenter += u->getPosition();
            groupCenter /= (float)myUnits.size();

            // 2. Calculate ONE path from Center -> Mouse Click
            std::vector<vec3> sharedPath = Pathfinder::findPath(groupCenter, clickPos, navGrid);

            // 3. Assign path, but give each unit a unique END POINT
            int count = myUnits.size();
            float spacing = 3.0f;

            for (int i = 0; i < count; i++) {
                // Calculate Formation Offset (Spiral)
                float radius = spacing * std::sqrt(i);
                float angle = i * 2.4f;
                vec3 offset(cos(angle) * radius, 0.0f, sin(angle) * radius);

                // Copy the shared path
                std::vector<vec3> myPersonalPath = sharedPath;

                //Change the LAST point to be their unique spot
                if (!myPersonalPath.empty()) {
                    myPersonalPath.back() = clickPos + offset;
                }
                else {
                    // If path was empty (very close), just push the single point
                    myPersonalPath.push_back(clickPos + offset);
                }

                myUnits[i]->clearTasks();
                myUnits[i]->setPath(myPersonalPath);
            }

            // Visual cleanup
            auto& allObs = environment->getObstacles();
            for (auto& o : allObs) o.marked = false;

            if (currentMode != InputMode::UNIT_SELECT) {
                currentMode = InputMode::UNIT_SELECT;
            }
        }
    }
    wasRightPressed = isRightPressed;

    // -------------------------------------------------------
    // 4. CAMERA MODE TOGGLE ('C')
    // -------------------------------------------------------
    static bool lastC = false;
    bool pressC = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
    if (pressC && !lastC) {
        if (unitCameraMode) {
            unitCameraMode = false;
        }
        else {
            focusedUnit = nullptr;
            for (auto& u : units) {
                if (u->isSelected()) {
                    focusedUnit = u.get();
                    unitCameraMode = true;
                    break;
                }
            }
        }
    }
    lastC = pressC;

    static bool lastG = false;
    bool keyG = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;

    if (keyG && !lastG) {
        for (auto& u : units) {
            if (u->isSelected() && u->getType() == UnitType::MELEE) {
                glm::vec3 pos = u->getPosition();
                float radius = 12.0f;

                // --- 1. PHYSICAL HOLE (Deform the actual 3D Mesh) ---
                // --- 1. PHYSICAL HOLE (Deform the actual 3D Mesh) ---
                terrain->createHole(pos, radius, 4.0f); // 4.0 units deep

                // --- 3. GAMEPLAY & PARTICLES ---
                navGrid->updateArea(pos, radius, true); // Block pathfinding
                ParticleManager::addExplosion(pos);     // Fire visuals
                u->explode();                           // Kill unit
            }
        }
    }
    lastG = keyG;
}


// Render: Draw the Drag Box (Green for Select, Yellow for Harvest)
void drawSelectionBox() {
    if (!isDragging) return;

    glDisable(GL_DEPTH_TEST);
    glUseProgram(0);

    mat4 P = camera->projectionMatrix;
    mat4 V = camera->viewMatrix;
    vec2 startScreen = worldToScreen(dragStartWorld, V, P);
    vec2 endScreen = worldToScreen(dragEndWorld, V, P);

    if (startScreen.x == -1 || endScreen.x == -1) return;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, W_WIDTH, W_HEIGHT, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // COLOR BASED ON MODE
    vec3 col;
    if (currentMode == InputMode::UNIT_SELECT) col = vec3(0.0f, 1.0f, 0.0f);     // Green
    else if (currentMode == InputMode::RESOURCE_SELECT) col = vec3(1.0f, 1.0f, 0.0f); // Yellow
    else col = vec3(1.0f, 0.0f, 0.0f); // Red (Attack)

    // Outline
    glColor4f(col.x, col.y, col.z, 1.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(startScreen.x, startScreen.y);
    glVertex2f(endScreen.x, startScreen.y);
    glVertex2f(endScreen.x, endScreen.y);
    glVertex2f(startScreen.x, endScreen.y);
    glEnd();

    // Fill
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(col.x, col.y, col.z, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(startScreen.x, startScreen.y);
    glVertex2f(endScreen.x, startScreen.y);
    glVertex2f(endScreen.x, endScreen.y);
    glVertex2f(startScreen.x, endScreen.y);
    glEnd();
    glDisable(GL_BLEND);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

// Render: Selection Rings under units
void drawSelectionRings(mat4 V, mat4 P) {
    //glDisable(GL_DEPTH_TEST);
    glUseProgram(0);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(&P[0][0]);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(&V[0][0]);

    // ---------------------------------------------------------
    // 1. Rings for SELECTED UNITS (Status Colors)
    // ---------------------------------------------------------
    for (const auto& u : units) {
        if (u->isSelected()) {
            vec3 pos = u->getPosition();

            // Dynamic Color Logic
            if (u->isGathering()) glColor3f(1.0f, 1.0f, 0.0f);      // Yellow (Working)
            else if (u->isAttacking() || u->isAttackingBuilding()) glColor3f(1.0f, 0.0f, 0.0f); // Red (Fighting)
            else glColor3f(0.0f, 1.0f, 0.0f);                       // Green (Idle)

            glLineWidth(2.0f);
            glBegin(GL_LINE_LOOP);
            float r = 2.5f;
            for (int i = 0; i < 16; i++) {
                float theta = 2.0f * 3.14159f * float(i) / 16.0f;
                float x = r * cosf(theta);
                float z = r * sinf(theta);
                glVertex3f(pos.x + x, pos.y + 0.5f, pos.z + z);
            }
            glEnd();
        }
    }

    // ---------------------------------------------------------
    // 2. Rings for TARGETS (When in Attack Mode)
    // ---------------------------------------------------------
    if (currentMode == InputMode::ATTACK_SELECT) {

        // A. Enemy Units
        for (const auto& u : units) {
            if (u->getTeam() == 1) { // Enemy
                vec3 pos = u->getPosition();
                glColor3f(1.0f, 0.0f, 0.0f); // Red Ring
                glLineWidth(2.0f);
                glBegin(GL_LINE_LOOP);
                float r = 3.0f;
                for (int i = 0; i < 16; i++) {
                    float theta = 2.0f * 3.14159f * float(i) / 16.0f;
                    float x = r * cosf(theta);
                    float z = r * sinf(theta);
                    glVertex3f(pos.x + x, pos.y + 0.5f, pos.z + z);
                }
                glEnd();
            }
        }

        // B. Enemy Buildings (NEW)
        for (const auto& b : buildings) {
            if (b->getTeam() == 1) { // Enemy Building
                vec3 pos = b->getPosition();
                glColor3f(1.0f, 0.0f, 0.0f); // Red Ring
                glLineWidth(3.0f);           // Thicker for buildings
                glBegin(GL_LINE_LOOP);

                // Larger Radius for buildings (matches Town Center size roughly)
                float r = 18.0f;

                for (int i = 0; i < 32; i++) {
                    float theta = 2.0f * 3.14159f * float(i) / 32.0f;
                    float x = r * cosf(theta);
                    float z = r * sinf(theta);
                    glVertex3f(pos.x + x, pos.y + 1.0f, pos.z + z);
                }
                glEnd();
            }
        }
    }

    glEnable(GL_DEPTH_TEST);
}

// Render: Resource Rings (Yellow)
void drawResourceRings(mat4 V, mat4 P) {
    glDisable(GL_DEPTH_TEST);
    glUseProgram(0);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(&P[0][0]);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(&V[0][0]);

    // Use the getter from Environment
    const auto& obstacles = environment->getObstacles();

    for (const auto& obs : obstacles) {
        // Draw only if Active AND Marked
        if (obs.active && obs.marked) {

            // Yellow Color
            glColor3f(1.0f, 1.0f, 0.0f);
            glLineWidth(2.0f);

            glBegin(GL_LINE_LOOP);
            // Radius slightly larger than the object
            float r = obs.radius + 1.5f;

            for (int i = 0; i < 16; i++) {
                float theta = 2.0f * 3.14159f * float(i) / 16.0f;
                float x = r * cosf(theta);
                float z = r * sinf(theta);

                // Lift Y by 1.0f to ensure it sits on top of the grass
                glVertex3f(obs.position.x + x, obs.position.y + 1.0f, obs.position.z + z);
            }
            glEnd();
        }
    }
    glEnable(GL_DEPTH_TEST);
}

// Handle building placement
void updateBuildingPlacement()
{
    static bool last1 = false, last2 = false, last3 = false;
    bool key1 = glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;
    bool key2 = glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS;
    bool key3 = glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS;

    // -----------------------------------------------------------
    // 1. DETERMINE RADIUS (Move this up so we can use it for checking)
    // -----------------------------------------------------------
    // We need to know the size BEFORE we click to check collisions.
    float buildingRadius = 12.0f;
    if (currentPlaceType == BuildingType::TOWN_CENTER) buildingRadius = 15.0f;
    else if (currentPlaceType == BuildingType::BARRACKS) buildingRadius = 12.0f;
    else if (currentPlaceType == BuildingType::SHOOTING_RANGE) buildingRadius = 10.0f;

    // -----------------------------------------------------------
    // 2. START PLACEMENT
    // -----------------------------------------------------------
    if (key1 && !last1) {
        placingBuilding = true;
        currentPlaceType = BuildingType::TOWN_CENTER;
        vec3 pos = getWorldPosUnderCursor();
        pos.y = 0.0f;
        previewBuilding = std::make_unique<Building>(currentPlaceType, pos, 0);
    }
    if (key2 && !last2) {
        placingBuilding = true;
        currentPlaceType = BuildingType::BARRACKS;
        vec3 pos = getWorldPosUnderCursor();
        pos.y = 0.0f;
        previewBuilding = std::make_unique<Building>(currentPlaceType, pos, 0);
    }
    if (key3 && !last3) {
        placingBuilding = true;
        currentPlaceType = BuildingType::SHOOTING_RANGE;
        vec3 pos = getWorldPosUnderCursor();
        pos.y = 0.0f;
        previewBuilding = std::make_unique<Building>(currentPlaceType, pos, 0);
    }
    last1 = key1; last2 = key2; last3 = key3;

    // -----------------------------------------------------------
    // 3. CHECK VALIDITY & UPDATE PREVIEW
    // -----------------------------------------------------------

    // Default to false (safety)
    isPlacementValid = false;

    if (placingBuilding && previewBuilding) {
        vec3 pos = getWorldPosUnderCursor();
        pos.y = 0.0f;
        previewBuilding->setPosition(pos);

        // Calculate Radius (matches your building types)
        float buildingRadius = 12.0f;
        if (currentPlaceType == BuildingType::TOWN_CENTER) buildingRadius = 15.0f;
        else if (currentPlaceType == BuildingType::BARRACKS) buildingRadius = 12.0f;
        else if (currentPlaceType == BuildingType::SHOOTING_RANGE) buildingRadius = 10.0f;

        // VALIDITY CHECK LOOP
        // Assume valid until we hit a block
        bool valid = true;

        if (navGrid) {
            int range = (int)buildingRadius;

            for (int x = -range; x <= range; x += 2) {
                for (int z = -range; z <= range; z += 2) {

                    // Circular check
                    if (glm::length(glm::vec2(x, z)) > buildingRadius) continue;

                    int gridX = (int)pos.x + x;
                    int gridZ = (int)pos.z + z;

                    // Check Bounds & Collisions
                    if (gridX < 0 || gridX >= 512 || gridZ < 0 || gridZ >= 512 ||
                        navGrid->isBlocked(gridX, gridZ)) {
                        valid = false;
                        break;
                    }
                }
                if (!valid) break;
            }
        }

        // Update the global variable for the renderer to see
        isPlacementValid = valid;
    }

    // -----------------------------------------------------------
    // 4. CONFIRM PLACEMENT (Left Click)
    // -----------------------------------------------------------
    static bool lastClick = false;
    bool clicked = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    if (clicked && !lastClick && placingBuilding) {

        // BLOCK PLACEMENT IF INVALID
        if (!isPlacementValid) {
            std::cout << "Cannot place here: Area blocked!" << std::endl;
        }
        else {
            ResourceCost cost = Building::getStaticCost(currentPlaceType);

            if (playerResources.spend(cost.wood, cost.rock)) {
                vec3 pos = previewBuilding->getPosition();

                // Determine radius again (or just reuse logic)
                float r = (currentPlaceType == BuildingType::TOWN_CENTER) ? 15.0f : 12.0f;

                terrain->flattenArea(pos, r);

                // Create the real building
                buildings.push_back(std::make_unique<Building>(currentPlaceType, pos, 0));

                if (navGrid) {
                    navGrid->updateArea(pos, r, true);
                }

                placingBuilding = false;
                previewBuilding = nullptr;
            }
            else {
                std::cout << "Not enough resources." << std::endl;
            }
        }
    }

    // Right Click Cancel
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        placingBuilding = false;
        previewBuilding = nullptr;
    }

    lastClick = clicked;
}

void drawBuildingUI()
{
    // -----------------------------------------------------------
    // SAFETY BLOCK (Crucial for 2D UI)
    // -----------------------------------------------------------
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);  
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D); 

    glUseProgram(0);

    mat4 P = camera->projectionMatrix;
    mat4 V = camera->viewMatrix;

    for (const auto& b : buildings) {

        float offsetHeight = 40.0f;
        const float barWidth = 80.0f;
        const float barHeight = 15.0f;
        const float constructionRadius = 20.0f;

        vec3 worldPos = b->getPosition();
        worldPos.y += offsetHeight;

        vec4 clipPos = P * V * vec4(worldPos, 1.0f);
        if (clipPos.w <= 0.0f) continue;

        vec3 ndc = vec3(clipPos) / clipPos.w;
        float screenX = (ndc.x + 1.0f) * 0.5f * W_WIDTH;
        float screenY = (1.0f - ndc.y) * 0.5f * W_HEIGHT;

        if (screenX < -150 || screenX > W_WIDTH + 150 ||
            screenY < -150 || screenY > W_HEIGHT + 150) continue;

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, W_WIDTH, W_HEIGHT, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        // DRAW CONSTRUCTION
        if (!b->isConstructed()) {
            float progress = b->getBuildProgress();

            glColor4f(0.2f, 0.2f, 0.2f, 0.7f);
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(screenX, screenY);
            for (int i = 0; i <= 32; ++i) {
                float angle = (i / 32.0f) * 2.0f * 3.14159f;
                glVertex2f(screenX + cos(angle) * constructionRadius, screenY + sin(angle) * constructionRadius);
            }
            glEnd();

            glColor4f(0.3f, 0.8f, 0.3f, 0.8f);
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(screenX, screenY);
            int segments = static_cast<int>(32 * progress);
            for (int i = 0; i <= segments; ++i) {
                float angle = (i / 32.0f) * 2.0f * 3.14159f - 3.14159f / 2.0f;
                glVertex2f(screenX + cos(angle) * constructionRadius, screenY + sin(angle) * constructionRadius);
            }
            glEnd();
        }

        // DRAW HEALTH BAR
        if (b->isConstructed() && b->getCurrentHealth() < b->getMaxHealth()) {

            float healthPercent = b->getCurrentHealth() / b->getMaxHealth();
            if (healthPercent < 0) healthPercent = 0; // Safety clamp

            vec3 hpColor(0.0f, 1.0f, 0.0f); // Green
            if (healthPercent < 0.5f) hpColor = vec3(1.0f, 1.0f, 0.0f); // Yellow
            if (healthPercent < 0.25f) hpColor = vec3(1.0f, 0.0f, 0.0f); // Red

            // Background (Dark)
            glColor4f(0.2f, 0.0f, 0.0f, 0.8f);
            glBegin(GL_QUADS);
            glVertex2f(screenX - barWidth / 2, screenY);
            glVertex2f(screenX + barWidth / 2, screenY);
            glVertex2f(screenX + barWidth / 2, screenY + barHeight);
            glVertex2f(screenX - barWidth / 2, screenY + barHeight);
            glEnd();

            // Foreground (HP)
            glColor4f(hpColor.r, hpColor.g, hpColor.b, 1.0f);
            glBegin(GL_QUADS);
            glVertex2f(screenX - barWidth / 2, screenY);
            glVertex2f(screenX - barWidth / 2 + barWidth * healthPercent, screenY);
            glVertex2f(screenX - barWidth / 2 + barWidth * healthPercent, screenY + barHeight);
            glVertex2f(screenX - barWidth / 2, screenY + barHeight);
            glEnd();

            // Border
            glLineWidth(2.0f);
            glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(screenX - barWidth / 2, screenY);
            glVertex2f(screenX + barWidth / 2, screenY);
            glVertex2f(screenX + barWidth / 2, screenY + barHeight);
            glVertex2f(screenX - barWidth / 2, screenY + barHeight);
            glEnd();
        }

        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
    }

    // RESTORE STATE
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE); // Turn back on for 3D objects
}

void drawCursor()
{
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(0);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, W_WIDTH, W_HEIGHT, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glColor3f(1.0f, 1.0f, 1.0f);
    glLineWidth(4.0f);
    const float len = 20.0f;
    glBegin(GL_LINES);
    glVertex2f(static_cast<float>(mx - len), static_cast<float>(my));
    glVertex2f(static_cast<float>(mx + len), static_cast<float>(my));
    glVertex2f(static_cast<float>(mx), static_cast<float>(my - len));
    glVertex2f(static_cast<float>(mx), static_cast<float>(my + len));
    glEnd();
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glEnable(GL_DEPTH_TEST);
}

void initialize()
{
    if (!glfwInit()) throw std::runtime_error("GLFW init failed");
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    // Use Compatibility Profile to support glBegin/glEnd logic
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_FALSE);

    window = glfwCreateWindow(W_WIDTH, W_HEIGHT, TITLE, nullptr, nullptr);
    if (!window) { glfwTerminate(); throw std::runtime_error("Window creation failed"); }
    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) throw std::runtime_error("GLEW init failed");
    glGetError(); // Clear false positive GLEW error

    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

    // MOUSE MUST BE NORMAL FOR RTS
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // --- BUILDINGS ---
    texTownCenter = loadSOIL("models/hexagons_medieval.png"); // Check your file names!
    texBarracks = loadSOIL("models/hexagons_medieval.png");
    texArchery = loadSOIL("models/hexagons_medieval.png");

    // --- NATURE ---
    texTree = loadSOIL("models/hexagons_medieval.png");
    texRock = loadSOIL("models/hexagons_medieval.png");

    fireTexture = loadSOIL("models/fire.png");
    glBindTexture(GL_TEXTURE_2D, fireTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (texTownCenter == 0) std::cout << "FAILED to load texTownCenter texture!" << std::endl;
    if (texBarracks == 0) std::cout << "FAILED to load texBarracks texture!" << std::endl;
    if (texArchery == 0) std::cout << "FAILED to load texArchery texture!" << std::endl;
    if (texTree == 0) std::cout << "FAILED to load texTree texture!" << std::endl;
    if (texRock == 0) std::cout << "FAILED to load texRock texture!" << std::endl;


    texWorker = loadSOIL("models/skeleton_texture.png");  
    texWarrior = loadSOIL("models/skeleton_texture.png"); 
    texMage = loadSOIL("models/skeleton_texture.png");    

    // Error checking
    if (texWorker == 0) std::cout << "FAILED to load Worker texture!" << std::endl;
    if (texWarrior == 0) std::cout << "FAILED to load Warrior texture!" << std::endl;
    if (texMage == 0) std::cout << "FAILED to load Mage texture!" << std::endl;

    camera = new Camera(window);
    glClearColor(0.5f, 0.5f, 0.5f, 0.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    logGLParameters();

    // =========================================================
    // LOAD ANIMATIONS 
    // =========================================================
    std::cout << "--- Loading Unit Meshes & Animations ---" << std::endl;

    // 1. Initialize the Base Meshes (The "Skin")
    // Ensure "models/minion_mesh.fbx" exists! (Or whatever your base mesh is named)
    if (!Unit::minionMesh)  Unit::minionMesh = new SkinnedMesh("models/Skeleton_Minion.fbx");
    if (!Unit::warriorMesh) Unit::warriorMesh = new SkinnedMesh("models/Skeleton_Warrior.fbx"); // Reusing for now
    if (!Unit::mageMesh)    Unit::mageMesh = new SkinnedMesh("models/Skeleton_Mage.fbx"); // Reusing for now

    // 2. Load External Animations (The "Moves")
    // Adjust the path string if your folder structure is different.

    // --- WORKER ANIMATIONS ---
    // General.fbx: [7] is "Idle_A"
    Unit::minionMesh->LoadAnimation("models/Animations/Rig_Medium_General.fbx", "IDLE", 7);
    // Movement.fbx: [8] is "Walking_A"
    Unit::minionMesh->LoadAnimation("models/Animations/Rig_Medium_MovementBasic.fbx", "WALK", 8);
    // Melee.fbx: [1] is "Melee_1H_Attack_Chop" (Good for workers)
    Unit::minionMesh->LoadAnimation("models/Animations/Rig_Medium_CombatMelee.fbx", "ATTACK", 1);

    Unit::minionMesh->PlayAnimation("IDLE"); // Set default

    // --- WARRIOR ANIMATIONS ---
    Unit::warriorMesh->LoadAnimation("models/Animations/Rig_Medium_General.fbx", "IDLE", 7);
    Unit::warriorMesh->LoadAnimation("models/Animations/Rig_Medium_MovementBasic.fbx", "WALK", 8);
    // Melee.fbx: [5] is "Melee_1H_Attack_Stab" (Good for swords)
    Unit::warriorMesh->LoadAnimation("models/Animations/Rig_Medium_CombatMelee.fbx", "ATTACK", 5);

    Unit::warriorMesh->PlayAnimation("IDLE");

    // --- MAGE ANIMATIONS ---
    Unit::mageMesh->LoadAnimation("models/Animations/Rig_Medium_General.fbx", "IDLE", 7);
    Unit::mageMesh->LoadAnimation("models/Animations/Rig_Medium_MovementBasic.fbx", "WALK", 8);
    // Ranged.fbx: [16] is "Ranged_Magic_Shoot" (Perfect for Mages)
    Unit::mageMesh->LoadAnimation("models/Animations/Rig_Medium_CombatRanged.fbx", "SHOOT", 16);

    Unit::mageMesh->PlayAnimation("IDLE");

    std::cout << "--- Animations Loaded ---" << std::endl;

}

void mainLoop() {
    float lastTime = static_cast<float>(glfwGetTime());
    const float cycleSpeed = 0.002f;

    static bool showDepthMap = false;
    static bool lastF1 = false;

    static float unitCameraAngle = 0; // Start facing unit front

    do {
        float currentTime = static_cast<float>(glfwGetTime());
        float dt = currentTime - lastTime;
        lastTime = currentTime;

        // 1. UPDATE SUN 
        float angle = currentTime * cycleSpeed;
        float tiltAngle = radians(45.0f);
        light.dir = normalize(vec3(
            sin(angle),
            cos(angle) * cos(tiltAngle),
            cos(angle+3.14) * sin(tiltAngle)
        ));

        float elevation = light.dir.y;
        if (elevation > 0.0f) {
            light.power = glm::mix(0.4f, 1.0f, elevation);
            vec4 warmColor = vec4(1.0f, 0.6f, 0.3f, 1.0f);
            vec4 neutralColor = vec4(1.0f, 0.95f, 0.9f, 1.0f);
            float warmthFactor = glm::smoothstep(0.0f, 0.7f, 1.0f - elevation);
            light.Ld = mix(neutralColor, warmColor, warmthFactor);
            light.La = mix(vec4(0.3f, 0.35f, 0.45f, 1.0f), vec4(0.4f, 0.3f, 0.2f, 1.0f), warmthFactor * 0.8f);
        }
        else {
            light.power = 0.15f;
            light.Ld = vec4(0.4f, 0.4f, 0.7f, 1.0f);
            light.La = vec4(0.05f, 0.05f, 0.1f, 1.0f);
        }

        // 2. SKY COLOR
        vec3 skyColor;
        if (elevation > 0.0f) {
            float horizonFactor = glm::pow(1.0f - elevation, 2.5f);
            skyColor = mix(vec3(0.53f, 0.78f, 0.95f), vec3(0.95f, 0.70f, 0.50f), horizonFactor);
        }
        else {
            float nightFactor = glm::smoothstep(-1.0f, 0.0f, elevation);
            skyColor = mix(vec3(0.01f, 0.01f, 0.05f), vec3(0.05f, 0.05f, 0.15f), nightFactor * 0.5f);
        }
        glClearColor(skyColor.r, skyColor.g, skyColor.b, 1.0f);

        // -------------------------------------------------------
        // 3. UPDATE LOGIC 
        // -------------------------------------------------------

        // A. Handle Selection (Raycast / Drag)
        handleInput();

        // B. Update Standard Camera
        if (!unitCameraMode) {
            camera->update();
        }

        mat4 P = camera->projectionMatrix;
        mat4 V = camera->viewMatrix;

        // 2. Prepare Batches (Separate by Type AND Animation)
        // --- WORKERS ---
        std::vector<glm::mat4> worker_IDLE, worker_WALK, worker_ATTACK;
        // --- WARRIORS ---
        std::vector<glm::mat4> warrior_IDLE, warrior_WALK, warrior_ATTACK;
        // --- MAGES ---
        std::vector<glm::mat4> mage_IDLE, mage_WALK, mage_ATTACK;

        // 3. Collection Loop
        for (const auto& u : units) {
            if (!cameraFrustum.isSphereVisible(u->getPosition(), 3.0f)) continue;

            glm::mat4 model = glm::translate(glm::mat4(1.0f), u->getPosition());

            // Rotation
            glm::vec3 vel = u->getVelocity();
            if (glm::length(vel) > 0.1f) {
                glm::vec3 direction = glm::normalize(vel);
                float angle = atan2(direction.x, direction.z);
                model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
            }
            float scaleVal = 3.0f;
            model = glm::scale(model, glm::vec3(scaleVal));

            // --- SORTING LOGIC ---
            // 1. Determine Animation State for this unit
            int animState = 0; // 0=IDLE, 1=WALK, 2=ATTACK

            if (u->getState() == UnitState::ATTACKING || u->getState() == UnitState::ATTACKING_BUILDING || u->getState() == UnitState::GATHERING) {
                animState = 2; // ATTACK
            }
            else if (glm::length(vel) > 0.1f) {
                animState = 1; // WALK
            }
            else {
                animState = 0; // IDLE
            }

            // 2. Push to correct Batch
            if (u->getType() == UnitType::WORKER) {
                if (animState == 2) worker_ATTACK.push_back(model);
                else if (animState == 1) worker_WALK.push_back(model);
                else worker_IDLE.push_back(model);
            }
            else if (u->getType() == UnitType::MELEE) {
                if (animState == 2) warrior_ATTACK.push_back(model);
                else if (animState == 1) warrior_WALK.push_back(model);
                else warrior_IDLE.push_back(model);
            }
            else if (u->getType() == UnitType::RANGED) {
                if (animState == 2) mage_ATTACK.push_back(model);
                else if (animState == 1) mage_WALK.push_back(model);
                else mage_IDLE.push_back(model);
            }
        }

        // C. Camera Override (Unit Camera)
        if (unitCameraMode && focusedUnit) {
            
            // 1. Handle Q/E Rotation (Only affects this mode)
            float rotSpeed = 2.0f;
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) unitCameraAngle -= rotSpeed * dt;
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) unitCameraAngle += rotSpeed * dt;

            // 2. Calculate Orbit Position
            vec3 uPos = focusedUnit->getPosition();
            float dist = 10.0f;
            float height = 6.0f;

            // Orbit math: Rotate offset around Y axis
            float offsetX = sin(unitCameraAngle) * dist;
            float offsetZ = cos(unitCameraAngle) * dist;

            vec3 eye = uPos + vec3(offsetX, height, offsetZ);
            vec3 target = uPos + vec3(0.0f, 2.0f, 0.0f); // Look at head height
            vec3 cameraUp = vec3(0.0f, 1.0f, 0.0f);

            V = glm::lookAt(eye, target, cameraUp);
        } 
        else {
            // Safety: If unit died or deselect, exit mode
            if (unitCameraMode && !focusedUnit) unitCameraMode = false;
        }
        cameraFrustum.update(P * V); // Combine Projection and View

        updateBuildingPlacement();

        // 1. Remove Dead Units (Clean up the vector)
        units.erase(std::remove_if(units.begin(), units.end(),
            [](const std::unique_ptr<Unit>& u) {
                return u->isDead();
            }), units.end());

        // 2. Update Remaining Units
        for (auto& u : units) {
            u->update(dt, terrain, units, playerResources, environment, navGrid);
        }
        

        // -------------------------------------------------------
        // UPDATE BUILDINGS & AUTO-SPAWN (With Spiral Formation)
        // -------------------------------------------------------
        for (auto& b : buildings) {
            b->updateConstruction(dt);

            UnitType typeToSpawn = b->updateAutoSpawning(dt);

            if ((int)typeToSpawn != -1) {

                // --- 1. Define Rally Point (In Front of Building) ---
                glm::vec3 buildingPos = b->getPosition();

                //  Calculate "Front" Vector based on Building Rotation (160 degrees)
                // We convert 160 degrees to radians to get the direction the door is facing.
                float rotationRadians = glm::radians(160.0f);

                // Assuming the mesh's natural forward is +Z (common for assets)
                // We rotate the vector (0,0,1) by 160 degrees.
                float dirX = sin(rotationRadians);
                float dirZ = cos(rotationRadians);

                glm::vec3 forwardDir(dirX, 0.0f, dirZ);

                // Move the center 30 units out along that direction
                glm::vec3 rallyCenter = buildingPos + (forwardDir * 10.0f);

                // --- 2. Calculate Spiral Offset ---
                // Use the number of units spawned so far (1 to 10) as the index 'i'
                int i = b->getSpawnedCount();
                float spacing = 3.0f; // Space between units

                // The Spiral Formula:
                float radius = spacing * std::sqrt(i);
                float angle = i * 2.4f; // Golden Angle for nice packing

                // Offset relative to the Rally Center
                glm::vec3 offset(cos(angle) * radius, 0.0f, sin(angle) * radius);

                // Final Target Position
                glm::vec3 spawnPos = rallyCenter + offset;

                // --- 3. Validate Position ---
                // Ensure we don't spawn inside a rock or tree
                if (navGrid && navGrid->isBlocked((int)spawnPos.x, (int)spawnPos.z)) {
                    spawnPos = Pathfinder::findNearestWalkable((int)spawnPos.x, (int)spawnPos.z, navGrid);
                }

                // --- 4. Spawn Unit ---
                if (spawnPos.x != -1.0f) {
                    auto newUnit = std::make_unique<Unit>(typeToSpawn, spawnPos, b->getTeam());

                    // Optional: Make them physically look away from the building initially
                    // newUnit->setRotation(160.0f); 

                    units.push_back(std::move(newUnit));
                }
            }
        }

        // -------------------------------------------------------
        // REMOVE DEAD BUILDINGS
        // -------------------------------------------------------
        auto it = buildings.begin();
        while (it != buildings.end()) {
            if ((*it)->isDead()) {
                // Unblock Grid
                float r = ((*it)->getType() == BuildingType::TOWN_CENTER) ? 12.0f : 8.0f;
                if (navGrid) {
                    navGrid->updateArea((*it)->getPosition(), r, false);
                }
                std::cout << "Building Destroyed!" << std::endl;
                it = buildings.erase(it);
            }
            else {
                ++it;
            }
        }

        // -------------------------------------------
        // RIGHT CLICK: Move Selected Units (With Formation)
        // -------------------------------------------
        static bool lastRightClick = false;
        bool rightClick = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

        if (rightClick && !lastRightClick) {
            vec3 clickPos = getWorldPosUnderCursor();

            // 1. Count selected units to calculate formation size
            std::vector<Unit*> selectedUnits;
            for (auto& u : units) {
                if (u->isSelected()) selectedUnits.push_back(u.get());
            }

            if (!selectedUnits.empty()) {
                int count = selectedUnits.size();
                float spacing = 3.5f; // Distance between units

                for (int i = 0; i < count; i++) {
                    // 2. Calculate offset (Spiral or Circle formation)
                    // Simple Circle Strategy:
                    float radius = spacing * std::sqrt(i + 1);
                    float angle = i * 2.4f; // Golden angle approx (distributes points evenly)

                    vec3 offset(cos(angle) * radius, 0.0f, sin(angle) * radius);
                    vec3 unitTarget = clickPos + offset;

                    // 3. Calculate Path for THIS specific unit to its unique target
                    // (This prevents them from fighting for the same end spot)
                    std::vector<vec3> path = Pathfinder::findPath(
                        selectedUnits[i]->getPosition(),
                        unitTarget,
                        navGrid // ✅ Now just passing the grid!
                    );
                    selectedUnits[i]->setPath(path);
                }
            }
        }
        lastRightClick = rightClick;

        // -------------------------------------------------------
        // 4. SHADOW MAP PASS
        // -------------------------------------------------------
        const float TERRAIN_SIZE = 512.0f;
        const float margin = 100.0f;
        mat4 lightProjection = ortho(-TERRAIN_SIZE / 2 - margin, TERRAIN_SIZE / 2 + margin,
            -TERRAIN_SIZE / 2 - margin, TERRAIN_SIZE / 2 + margin,
            0.1f, 1000.0f);

        vec3 lightTarget = vec3(256.0f, 0.0f, 256.0f);
        vec3 lightPos = lightTarget + light.dir * 500.0f;
        mat4 lightView = lookAt(lightPos, lightTarget, vec3(0, 1, 0));
        mat4 lightSpaceMatrix = lightProjection * lightView;

        shadowMap->bindForWriting();
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glClear(GL_DEPTH_BUFFER_BIT);

        // We render the BACK faces of objects into the shadow map.
        // This prevents acne without shrinking the shadow (Peter Panning).
        // 
        glCullFace(GL_FRONT);

        // We can now DISABLE the offset entirely!
        glDisable(GL_POLYGON_OFFSET_FILL);

        // A. STATIC OBJECTS (Terrain, Trees, Buildings) -> Use Standard Shadow Shader
        glUseProgram(shadowShader);
        glUniformMatrix4fv(glGetUniformLocation(shadowShader, "lightSpaceMatrix"), 1, GL_FALSE, &lightSpaceMatrix[0][0]);

        // 1. Terrain
        mat4 model = mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shadowShader, "model"), 1, GL_FALSE, &model[0][0]);
        terrain->draw(model, shadowShader);

        // 2. Environment (Trees/Rocks)
        GLint modelLocShadow = glGetUniformLocation(shadowShader, "model");
        environment->draw(shadowShader, modelLocShadow, cameraFrustum);

        // 3. Buildings
        for (const auto& b : buildings) {
            mat4 model = translate(mat4(1.0f), b->getPosition());

            // ROTATE 180 DEGREES (Add this line)
            model = glm::rotate(model, glm::radians(160.0f), glm::vec3(0.0f, 1.0f, 0.0f));

            float scale = (b->getType() == BuildingType::TOWN_CENTER) ? 10.0f : ((b->getType() == BuildingType::BARRACKS) ? 7.0f : 5.0f);
            model = glm::scale(model, vec3(scale));

            glUniformMatrix4fv(glGetUniformLocation(shadowShader, "model"), 1, GL_FALSE, &model[0][0]);
            b->getMesh()->draw();
        }

        // B. ANIMATED UNITS -> Use Instanced Shader (Handles Bones!)
        glUseProgram(instancedShader);

        // Pass "Sun" matrices so the shader draws from the light's perspective
        glUniformMatrix4fv(glGetUniformLocation(instancedShader, "P"), 1, GL_FALSE, &lightProjection[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(instancedShader, "V"), 1, GL_FALSE, &lightView[0][0]);

        // Optimization: Disable color writing (Shadow map only needs Depth)
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

        // ============================
        // WORKERS (SHADOWS)
        // ============================
        if (Unit::minionMesh) {
            // IDLE
            if (!worker_IDLE.empty()) {
                Unit::minionMesh->PlayAnimation("IDLE");
                Unit::minionMesh->UpdateAnimation(glfwGetTime());
                auto bones = Unit::minionMesh->GetFinalBoneMatrices();
                for (int i = 0; i < bones.size(); i++) glUniformMatrix4fv(glGetUniformLocation(instancedShader, ("finalBonesMatrices[" + std::to_string(i) + "]").c_str()), 1, GL_FALSE, &bones[i][0][0]);
                Unit::minionMesh->DrawInstanced(instancedShader, worker_IDLE);
            }
            // WALK
            if (!worker_WALK.empty()) {
                Unit::minionMesh->PlayAnimation("WALK");
                Unit::minionMesh->UpdateAnimation(glfwGetTime());
                auto bones = Unit::minionMesh->GetFinalBoneMatrices();
                for (int i = 0; i < bones.size(); i++) glUniformMatrix4fv(glGetUniformLocation(instancedShader, ("finalBonesMatrices[" + std::to_string(i) + "]").c_str()), 1, GL_FALSE, &bones[i][0][0]);
                Unit::minionMesh->DrawInstanced(instancedShader, worker_WALK);
            }
            // ATTACK
            if (!worker_ATTACK.empty()) {
                Unit::minionMesh->PlayAnimation("ATTACK");
                Unit::minionMesh->UpdateAnimation(glfwGetTime());
                auto bones = Unit::minionMesh->GetFinalBoneMatrices();
                for (int i = 0; i < bones.size(); i++) glUniformMatrix4fv(glGetUniformLocation(instancedShader, ("finalBonesMatrices[" + std::to_string(i) + "]").c_str()), 1, GL_FALSE, &bones[i][0][0]);
                Unit::minionMesh->DrawInstanced(instancedShader, worker_ATTACK);
            }
        }

        // ============================
        // WARRIORS (SHADOWS)
        // ============================
        if (Unit::warriorMesh) {
            if (!warrior_IDLE.empty()) {
                Unit::warriorMesh->PlayAnimation("IDLE");
                Unit::warriorMesh->UpdateAnimation(glfwGetTime());
                auto bones = Unit::warriorMesh->GetFinalBoneMatrices();
                for (int i = 0; i < bones.size(); i++) glUniformMatrix4fv(glGetUniformLocation(instancedShader, ("finalBonesMatrices[" + std::to_string(i) + "]").c_str()), 1, GL_FALSE, &bones[i][0][0]);
                Unit::warriorMesh->DrawInstanced(instancedShader, warrior_IDLE);
            }
            if (!warrior_WALK.empty()) {
                Unit::warriorMesh->PlayAnimation("WALK");
                Unit::warriorMesh->UpdateAnimation(glfwGetTime());
                auto bones = Unit::warriorMesh->GetFinalBoneMatrices();
                for (int i = 0; i < bones.size(); i++) glUniformMatrix4fv(glGetUniformLocation(instancedShader, ("finalBonesMatrices[" + std::to_string(i) + "]").c_str()), 1, GL_FALSE, &bones[i][0][0]);
                Unit::warriorMesh->DrawInstanced(instancedShader, warrior_WALK);
            }
            if (!warrior_ATTACK.empty()) {
                Unit::warriorMesh->PlayAnimation("ATTACK");
                Unit::warriorMesh->UpdateAnimation(glfwGetTime());
                auto bones = Unit::warriorMesh->GetFinalBoneMatrices();
                for (int i = 0; i < bones.size(); i++) glUniformMatrix4fv(glGetUniformLocation(instancedShader, ("finalBonesMatrices[" + std::to_string(i) + "]").c_str()), 1, GL_FALSE, &bones[i][0][0]);
                Unit::warriorMesh->DrawInstanced(instancedShader, warrior_ATTACK);
            }
        }

        // ============================
        // MAGES (SHADOWS)
        // ============================
        if (Unit::mageMesh) {
            if (!mage_IDLE.empty()) {
                Unit::mageMesh->PlayAnimation("IDLE");
                Unit::mageMesh->UpdateAnimation(glfwGetTime());
                auto bones = Unit::mageMesh->GetFinalBoneMatrices();
                for (int i = 0; i < bones.size(); i++) glUniformMatrix4fv(glGetUniformLocation(instancedShader, ("finalBonesMatrices[" + std::to_string(i) + "]").c_str()), 1, GL_FALSE, &bones[i][0][0]);
                Unit::mageMesh->DrawInstanced(instancedShader, mage_IDLE);
            }
            if (!mage_WALK.empty()) {
                Unit::mageMesh->PlayAnimation("WALK");
                Unit::mageMesh->UpdateAnimation(glfwGetTime());
                auto bones = Unit::mageMesh->GetFinalBoneMatrices();
                for (int i = 0; i < bones.size(); i++) glUniformMatrix4fv(glGetUniformLocation(instancedShader, ("finalBonesMatrices[" + std::to_string(i) + "]").c_str()), 1, GL_FALSE, &bones[i][0][0]);
                Unit::mageMesh->DrawInstanced(instancedShader, mage_WALK);
            }
            if (!mage_ATTACK.empty()) {
                Unit::mageMesh->PlayAnimation("SHOOT"); // Use SHOOT for attack
                Unit::mageMesh->UpdateAnimation(glfwGetTime());
                auto bones = Unit::mageMesh->GetFinalBoneMatrices();
                for (int i = 0; i < bones.size(); i++) glUniformMatrix4fv(glGetUniformLocation(instancedShader, ("finalBonesMatrices[" + std::to_string(i) + "]").c_str()), 1, GL_FALSE, &bones[i][0][0]);
                Unit::mageMesh->DrawInstanced(instancedShader, mage_ATTACK);
            }
        }

        // Restore State
        glCullFace(GL_BACK);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); // Re-enable color writing!
        //glDisable(GL_POLYGON_OFFSET_FILL);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, W_WIDTH, W_HEIGHT);

        // -------------------------------------------------------
        // 5. SNOW TRAIL PASS
        // -------------------------------------------------------
        snowTrailMap->bindForWriting();
        glDisable(GL_DEPTH_TEST);

        glDisable(GL_BLEND);
        glUseProgram(whiteShader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, snowTrailMap->getTexture());
        glUniform1i(glGetUniformLocation(whiteShader, "currentSnow"), 0);
        glBindVertexArray(quadVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(snowTrailShader);
        glEnable(GL_PROGRAM_POINT_SIZE);
        glBindVertexArray(dummyPointVAO);

        for (const auto& u : units) {
            mat4 model = translate(mat4(1.0f), u->getPosition());
            model = scale(model, vec3(8.0f));
            glUniformMatrix4fv(glGetUniformLocation(snowTrailShader, "model"), 1, GL_FALSE, &model[0][0]);
            glDrawArrays(GL_POINTS, 0, 1);
        }

        glBindVertexArray(0);
        glDisable(GL_PROGRAM_POINT_SIZE);
        glDisable(GL_BLEND);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, W_WIDTH, W_HEIGHT);

        // -------------------------------------------------------
        // 6. DEBUG (F1)
        // -------------------------------------------------------
        bool f1Pressed = glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS;
        if (f1Pressed && !lastF1) showDepthMap = !showDepthMap;
        lastF1 = f1Pressed;

        if (showDepthMap) {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glUseProgram(debugShader);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, shadowMap->getDepthTexture());
            glUniform1i(glGetUniformLocation(debugShader, "depthMap"), 0);
            glDisable(GL_DEPTH_TEST);
            glBindVertexArray(quadVAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            glEnable(GL_DEPTH_TEST);
            drawCursor();
            glfwSwapBuffers(window);
            glfwPollEvents();
            continue;
        }

        // -------------------------------------------------------
        // 7. MAIN RENDER PASS
        // -------------------------------------------------------

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, shadowMap->getDepthTexture());
        glActiveTexture(GL_TEXTURE6);
        snowTrailMap->bindForReading(GL_TEXTURE6);

        // A. Terrain
        glUseProgram(terrainShader);
        glUniformMatrix4fv(glGetUniformLocation(terrainShader, "P"), 1, GL_FALSE, &P[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(terrainShader, "V"), 1, GL_FALSE, &V[0][0]);
        mat4 terrainM = mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(terrainShader, "M"), 1, GL_FALSE, &terrainM[0][0]);
        glUniform3f(glGetUniformLocation(terrainShader, "lightDirection_worldspace"), light.dir.x, light.dir.y, light.dir.z);
        glUniform1f(glGetUniformLocation(terrainShader, "lightPower"), light.power);
        glUniformMatrix4fv(glGetUniformLocation(terrainShader, "lightSpaceMatrix"), 1, GL_FALSE, &lightSpaceMatrix[0][0]);
        glUniform1i(glGetUniformLocation(terrainShader, "shadowMap"), 5);
        glUniform1i(glGetUniformLocation(terrainShader, "snowTrailMap"), 6);
        terrain->draw(terrainM);

        // -------------------------------------------------------
        // B. STANDARD OBJECTS (Terrain, Buildings, Environment)
        // -------------------------------------------------------
        glUseProgram(shaderProgram);

        // 1. Prepare Texture Unit 0
        glActiveTexture(GL_TEXTURE0);

        // Tell Shader to use Textures
        glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 1);
        glUniform1i(glGetUniformLocation(shaderProgram, "diffuseColorSampler"), 0); // Read from Unit 0

        //  Reset Material Colors to WHITE
        glUniform4f(KdLocation, 1.0f, 1.0f, 1.0f, 1.0f);
        glUniform4f(KaLocation, 0.2f, 0.2f, 0.2f, 1.0f);
        glUniform4f(KsLocation, 0.1f, 0.1f, 0.1f, 1.0f);
        glUniform1f(NsLocation, 50.0f);

        // Matrix & Light Uniforms
        glUniformMatrix4fv(projectionMatrixLocation, 1, GL_FALSE, &P[0][0]);
        glUniformMatrix4fv(viewMatrixLocation, 1, GL_FALSE, &V[0][0]);
        glUniform3f(lightDirectionLocation, light.dir.x, light.dir.y, light.dir.z);
        glUniform1f(lightPowerLocation, light.power);
        glUniform4fv(LaLocation, 1, &light.La[0]);
        glUniform4fv(LdLocation, 1, &light.Ld[0]);
        glUniform4fv(LsLocation, 1, &light.Ls[0]);
        glUniformMatrix4fv(lightSpaceMatrixLoc_standard, 1, GL_FALSE, &lightSpaceMatrix[0][0]);

        // Shadow Map
        glUniform1i(shadowMapLoc_standard, 5);

        // Construction Uniforms (Reset defaults)
        glUniform1f(glGetUniformLocation(shaderProgram, "constructionProgress"), 1.0f);
        glUniform1f(glGetUniformLocation(shaderProgram, "buildingHeight"), 50.0f);
        glUniform3f(glGetUniformLocation(shaderProgram, "buildingBasePos"), 0, 0, 0);

        // DRAW ENVIRONMENT (Trees/Rocks)
        // Note: Environment.cpp handles binding its own textures internally
        int envDrawn = environment->draw(shaderProgram, modelMatrixLocation, cameraFrustum);
        

        // DRAW BUILDINGS
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);

        glActiveTexture(GL_TEXTURE0); // Ensure we are affecting Unit 0

        for (const auto& b : buildings) {
            if (cameraFrustum.isSphereVisible(b->getPosition(), 15.0f)) {

                // SWITCH TEXTURE BASED ON TYPE
                if (b->getType() == BuildingType::TOWN_CENTER) {
                    glBindTexture(GL_TEXTURE_2D, texTownCenter);
                }
                else if (b->getType() == BuildingType::BARRACKS) {
                    glBindTexture(GL_TEXTURE_2D, texBarracks);
                }
                else if (b->getType() == BuildingType::SHOOTING_RANGE) {
                    glBindTexture(GL_TEXTURE_2D, texArchery);
                }

                // Draw
                b->draw(V, P, shaderProgram, 1.0f);
            }
        }
        

        // -------------------------------------------------------
        // PREVIEW GHOST BUILDING (Hologram Style)
        // -------------------------------------------------------
        if (previewBuilding && placingBuilding) {

            // 1. Disable Texture (So it looks like a solid energy field)
            glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 0);

            // 2. Determine Color: Bright Green (Valid) or Bright Red (Invalid)
            glm::vec3 ghostColor = isPlacementValid ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);

            // 3. Draw with transparency (0.4f alpha is a bit more visible than 0.3f)
            previewBuilding->draw(V, P, shaderProgram, 0.4f, ghostColor);

            // 4. Re-enable Texture (CRITICAL: So the rest of the game looks normal next frame)
            glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 1);
        }

        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);

        // -------------------------------------------------------
        // C. SKINNED UNITS (INSTANCED)
        // -------------------------------------------------------
        glUseProgram(instancedShader);

        // 1. Prepare Texture Unit 0
        glActiveTexture(GL_TEXTURE0);

        // Enable Textures for Instanced Shader
        glUniform1i(glGetUniformLocation(instancedShader, "useTexture"), 1);
        glUniform1i(glGetUniformLocation(instancedShader, "diffuseColorSampler"), 0);

        // Global Uniforms
        glUniformMatrix4fv(glGetUniformLocation(instancedShader, "P"), 1, GL_FALSE, &P[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(instancedShader, "V"), 1, GL_FALSE, &V[0][0]);

        glUniform3f(glGetUniformLocation(instancedShader, "lightDirection_worldspace"), light.dir.x, light.dir.y, light.dir.z);
        glUniform1f(glGetUniformLocation(instancedShader, "light.power"), light.power);
        glUniform4fv(glGetUniformLocation(instancedShader, "light.La"), 1, &light.La[0]);
        glUniform4fv(glGetUniformLocation(instancedShader, "light.Ld"), 1, &light.Ld[0]);
        glUniform4fv(glGetUniformLocation(instancedShader, "light.Ls"), 1, &light.Ls[0]);
        glUniformMatrix4fv(glGetUniformLocation(instancedShader, "lightSpaceMatrix"), 1, GL_FALSE, &lightSpaceMatrix[0][0]);

        // Shadow Map (Unit 5)
        glUniform1i(glGetUniformLocation(instancedShader, "shadowMap"), 5);

        // Material Defaults (White so we see texture)
        glUniform1f(glGetUniformLocation(instancedShader, "constructionProgress"), 1.0f);
        glUniform1f(glGetUniformLocation(instancedShader, "buildingHeight"), 10.0f);
        glUniform3f(glGetUniformLocation(instancedShader, "buildingBasePos"), 0, 0, 0);
        glUniform4f(glGetUniformLocation(instancedShader, "mtl.Ka"), 0.2f, 0.2f, 0.2f, 1.0f);
        glUniform4f(glGetUniformLocation(instancedShader, "mtl.Kd"), 1.0f, 1.0f, 1.0f, 1.0f); // White
        glUniform4f(glGetUniformLocation(instancedShader, "mtl.Ks"), 0.5f, 0.5f, 0.5f, 1.0f);
        glUniform1f(glGetUniformLocation(instancedShader, "mtl.Ns"), 50.0f);

        // 4. Draw Batches
        // NOTE: Ensure SkinnedMesh::DrawInstanced binds textures to Unit 0!

        // ============================
        // WORKERS
        // ============================
        if (Unit::minionMesh) {
            glBindTexture(GL_TEXTURE_2D, texWorker);

            // 1. Draw IDLE Workers
            if (!worker_IDLE.empty()) {
                Unit::minionMesh->PlayAnimation("IDLE");
                Unit::minionMesh->UpdateAnimation(glfwGetTime()); // Calc IDLE bones

                // Send Bones
                auto bones = Unit::minionMesh->GetFinalBoneMatrices();
                for (int i = 0; i < bones.size(); i++) {
                    std::string name = "finalBonesMatrices[" + std::to_string(i) + "]";
                    glUniformMatrix4fv(glGetUniformLocation(instancedShader, name.c_str()), 1, GL_FALSE, &bones[i][0][0]);
                }
                Unit::minionMesh->DrawInstanced(instancedShader, worker_IDLE);
            }

            // 2. Draw WALKING Workers
            if (!worker_WALK.empty()) {
                Unit::minionMesh->PlayAnimation("WALK");
                Unit::minionMesh->UpdateAnimation(glfwGetTime()); // Calc WALK bones

                auto bones = Unit::minionMesh->GetFinalBoneMatrices();
                for (int i = 0; i < bones.size(); i++) {
                    std::string name = "finalBonesMatrices[" + std::to_string(i) + "]";
                    glUniformMatrix4fv(glGetUniformLocation(instancedShader, name.c_str()), 1, GL_FALSE, &bones[i][0][0]);
                }
                Unit::minionMesh->DrawInstanced(instancedShader, worker_WALK);
            }

            // 3. Draw ATTACKING Workers
            if (!worker_ATTACK.empty()) {
                Unit::minionMesh->PlayAnimation("ATTACK");
                Unit::minionMesh->UpdateAnimation(glfwGetTime());

                auto bones = Unit::minionMesh->GetFinalBoneMatrices();
                for (int i = 0; i < bones.size(); i++) {
                    std::string name = "finalBonesMatrices[" + std::to_string(i) + "]";
                    glUniformMatrix4fv(glGetUniformLocation(instancedShader, name.c_str()), 1, GL_FALSE, &bones[i][0][0]);
                }
                Unit::minionMesh->DrawInstanced(instancedShader, worker_ATTACK);
            }
        }

        // ============================
        // WARRIORS (Repeat Logic)
        // ============================
        if (Unit::warriorMesh) {
            glBindTexture(GL_TEXTURE_2D, texWarrior);

            if (!warrior_IDLE.empty()) {
                Unit::warriorMesh->PlayAnimation("IDLE");
                Unit::warriorMesh->UpdateAnimation(glfwGetTime());
                auto bones = Unit::warriorMesh->GetFinalBoneMatrices();
                for (int i = 0; i < bones.size(); i++) glUniformMatrix4fv(glGetUniformLocation(instancedShader, ("finalBonesMatrices[" + std::to_string(i) + "]").c_str()), 1, GL_FALSE, &bones[i][0][0]);
                Unit::warriorMesh->DrawInstanced(instancedShader, warrior_IDLE);
            }
            if (!warrior_WALK.empty()) {
                Unit::warriorMesh->PlayAnimation("WALK");
                Unit::warriorMesh->UpdateAnimation(glfwGetTime());
                auto bones = Unit::warriorMesh->GetFinalBoneMatrices();
                for (int i = 0; i < bones.size(); i++) glUniformMatrix4fv(glGetUniformLocation(instancedShader, ("finalBonesMatrices[" + std::to_string(i) + "]").c_str()), 1, GL_FALSE, &bones[i][0][0]);
                Unit::warriorMesh->DrawInstanced(instancedShader, warrior_WALK);
            }
            if (!warrior_ATTACK.empty()) {
                Unit::warriorMesh->PlayAnimation("ATTACK");
                Unit::warriorMesh->UpdateAnimation(glfwGetTime());
                auto bones = Unit::warriorMesh->GetFinalBoneMatrices();
                for (int i = 0; i < bones.size(); i++) glUniformMatrix4fv(glGetUniformLocation(instancedShader, ("finalBonesMatrices[" + std::to_string(i) + "]").c_str()), 1, GL_FALSE, &bones[i][0][0]);
                Unit::warriorMesh->DrawInstanced(instancedShader, warrior_ATTACK);
            }
        }

        // ============================
        // MAGES (Repeat Logic)
        // ============================
        if (Unit::mageMesh) {
            glBindTexture(GL_TEXTURE_2D, texMage);

            if (!mage_IDLE.empty()) {
                Unit::mageMesh->PlayAnimation("IDLE");
                Unit::mageMesh->UpdateAnimation(glfwGetTime());
                auto bones = Unit::mageMesh->GetFinalBoneMatrices();
                for (int i = 0; i < bones.size(); i++) glUniformMatrix4fv(glGetUniformLocation(instancedShader, ("finalBonesMatrices[" + std::to_string(i) + "]").c_str()), 1, GL_FALSE, &bones[i][0][0]);
                Unit::mageMesh->DrawInstanced(instancedShader, mage_IDLE);
            }
            if (!mage_WALK.empty()) {
                Unit::mageMesh->PlayAnimation("WALK");
                Unit::mageMesh->UpdateAnimation(glfwGetTime());
                auto bones = Unit::mageMesh->GetFinalBoneMatrices();
                for (int i = 0; i < bones.size(); i++) glUniformMatrix4fv(glGetUniformLocation(instancedShader, ("finalBonesMatrices[" + std::to_string(i) + "]").c_str()), 1, GL_FALSE, &bones[i][0][0]);
                Unit::mageMesh->DrawInstanced(instancedShader, mage_WALK);
            }
            if (!mage_ATTACK.empty()) {
                // Mages use "SHOOT" usually, but we mapped it to "SHOOT" in initialize
                Unit::mageMesh->PlayAnimation("SHOOT");
                Unit::mageMesh->UpdateAnimation(glfwGetTime());
                auto bones = Unit::mageMesh->GetFinalBoneMatrices();
                for (int i = 0; i < bones.size(); i++) glUniformMatrix4fv(glGetUniformLocation(instancedShader, ("finalBonesMatrices[" + std::to_string(i) + "]").c_str()), 1, GL_FALSE, &bones[i][0][0]);
                Unit::mageMesh->DrawInstanced(instancedShader, mage_ATTACK);
            }
        }

        // Restore standard shader for UI/Lines
        glUseProgram(shaderProgram);

        // -------------------------------------------------------
        // 8. PARTICLE PASS (Add this after Units)
        // -------------------------------------------------------
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive "Glow"
        glDepthMask(GL_FALSE);             // Don't hide particles behind each other

        mat4 PV = P * V;
        // Ensure particleShaderProgram is used
        ParticleManager::updateAndRender(dt, camera->position, PV, particleShaderProgram, fireTexture);

        glDepthMask(GL_TRUE);              // Reset depth writing
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Reset blending
        glDisable(GL_BLEND);
        

        // -------------------------------------------------------
        // 8. UI OVERLAYS (Added Back)
        // -------------------------------------------------------
        drawSelectionRings(V, P); // Draw rings FIRST (behind UI)
        drawSelectionBox();       // Draw drag box
        drawResourceRings(V, P);
        drawBuildingUI();
        drawCursor();

        glfwSwapBuffers(window);
        glfwPollEvents();

    } while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
        glfwWindowShouldClose(window) == 0);
}

void freeResources()
{
    // 1. Clear Game Objects FIRST (while OpenGL context is still alive)
    units.clear();
    buildings.clear();

    // ✅ ADDED: Clean up static SkinnedMesh pointers
    // Since these were created with 'new', they must be deleted manually
    delete Unit::minionMesh;  Unit::minionMesh = nullptr;
    delete Unit::warriorMesh; Unit::warriorMesh = nullptr;
    delete Unit::mageMesh;    Unit::mageMesh = nullptr;

    // ✅ ADDED: Clean up Particle Manager
    // Clear the active emitters and delete the shared quad/sphere model
    ParticleManager::active_emitters.clear();
    delete ParticleManager::particle_quad;
    ParticleManager::particle_quad = nullptr;

    // 2. Delete Systems
    delete environment; environment = nullptr;
    delete terrain; terrain = nullptr;
    delete camera; camera = nullptr;
    delete navGrid; navGrid = nullptr;

    // 3. Delete OpenGL Resources
    glDeleteProgram(shaderProgram);
    glDeleteProgram(terrainShader);
    glDeleteProgram(shadowShader);
    glDeleteProgram(debugShader);
    glDeleteProgram(snowTrailShader);
    glDeleteProgram(whiteShader);

    // ✅ ADDED: Delete the Particle Shader
    glDeleteProgram(particleShaderProgram);
    //glDeleteProgram(skinnedShader);
    glDeleteProgram(instancedShader);

    delete shadowMap; shadowMap = nullptr;
    delete snowTrailMap; snowTrailMap = nullptr;

    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteBuffers(1, &quadEBO);
    glDeleteVertexArrays(1, &dummyPointVAO);
    glDeleteBuffers(1, &dummyPointVBO);

    // ✅ ADDED: Delete Textures 
    // It's good practice to free the GPU memory used by textures
    GLuint textures[] = {
        texWorker, texWarrior, texMage,
        texTownCenter, texBarracks, texArchery,
        texTree, texRock, fireTexture
    };
    glDeleteTextures(sizeof(textures) / sizeof(GLuint), textures);

    // 4. Kill Context
    glfwTerminate();
}

int main()
{
    try {
        initialize();
        createContext();
        mainLoop();
        freeResources();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        std::cin.get();
        freeResources();
        return -1;
    }
    return 0;
}