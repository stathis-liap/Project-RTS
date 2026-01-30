// ---------------------------------------------------------------
// main.cpp
// ---------------------------------------------------------------
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
#include <glm/gtc/type_ptr.hpp> 
#include "Terrain.h"
#include "ShadowMap.h"
#include "Mesh.h"
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

// ---------------------------------------------------------------
using namespace std;
using namespace glm;
// ---------------------------------------------------------------
#define W_WIDTH 1920
#define W_HEIGHT 1080
#define TITLE "RTS – Terrain + Cursor + Zoom-to-cursor"
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

Environment* environment = nullptr;

SkinnedMesh* myActor = nullptr;
GLuint skinnedShader = 0;

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
    vec4{0.1f,0.1f,0.1f,1}, vec4{1.0f,0.8f,0.0f,1},
    vec4{0.3f,0.3f,0.3f,1}, 20.0f
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

    // Note: If Terrain doesn't have getHeight(), pass nullptr or update Terrain class
    navGrid = new NavigationGrid(512, 512);

    environment->initialize(terrain, 512.0f, 1500);


    shadowShader = loadShaders("ShadowMap.vertexshader", "ShadowMap.fragmentshader");

    shadowMap = new ShadowMap(2048, 2048);


    snowTrailMap = new SnowTrailMap(1024, 1024);

    snowTrailMap->clear(); // ← FULL SNOW at start


    snowTrailShader = loadShaders("SnowTrail.vertexshader", "SnowTrail.fragmentshader");


    debugShader = loadShaders("debugDepthShader.vertexshader", "debugDepthShader.fragmentshader");


    whiteShader = loadShaders("White.vertexshader", "White.fragmentshader");


    skinnedShader = loadShaders("SkinnedShading.vertexshader", "SkinnedShading.fragmentshader");


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


    // ← DUMMY VAO/VBO for snow trail points

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

    terrain->flattenArea(myTownCenterPos, 15.0f);  // ✅ Flatten first

    buildings.push_back(std::make_unique<Building>(BuildingType::TOWN_CENTER, myTownCenterPos));

    buildings.back()->updateConstruction(10000.0f);


    vec3 enemyTownCenterPos = vec3(460.0f, 0.0f, 460.0f);

    terrain->flattenArea(enemyTownCenterPos, 15.0f);  // ✅ Flatten first

    buildings.push_back(std::make_unique<Building>(BuildingType::TOWN_CENTER, enemyTownCenterPos));

    buildings.back()->updateConstruction(10000.0f);


    // Old way:
    // units.push_back(std::make_unique<Unit>(UnitType::WORKER, vec3(260.0f, 0.0f, 260.0f)));

    // ✅ New Safe Way: Check grid before spawning
    vec3 workerPos(260.0f, 0.0f, 260.0f);
    vec3 warriorPos(245.0f, 0.0f, 245.0f);
    vec3 magePos(230.0f, 0.0f, 230.0f);

    // If blocked, search for a valid spot
    if (navGrid->isBlocked((int)workerPos.x, (int)workerPos.z)) {
        vec3 safePos = Pathfinder::findNearestWalkable((int)workerPos.x, (int)workerPos.z, navGrid);
        if (safePos.x != -1.0f) workerPos = safePos;
    }
    units.push_back(std::make_unique<Unit>(UnitType::WORKER, workerPos, 0));

    if (navGrid->isBlocked((int)warriorPos.x, (int)warriorPos.z)) {
        vec3 safePos = Pathfinder::findNearestWalkable((int)warriorPos.x, (int)warriorPos.z, navGrid);
        if (safePos.x != -1.0f) warriorPos = safePos;
    }
    units.push_back(std::make_unique<Unit>(UnitType::MELEE, warriorPos, 0));

    if (navGrid->isBlocked((int)magePos.x, (int)magePos.z)) {
        vec3 safePos = Pathfinder::findNearestWalkable((int)magePos.x, (int)magePos.z, navGrid);
        if (safePos.x != -1.0f) magePos = safePos;
    }
    units.push_back(std::make_unique<Unit>(UnitType::RANGED, magePos, 0));

    units.push_back(std::make_unique<Unit>(UnitType::MELEE, vec3(300, 0, 300), 1));
    units.push_back(std::make_unique<Unit>(UnitType::RANGED, vec3(310, 0, 310), 1));
    units.push_back(std::make_unique<Unit>(UnitType::WORKER, vec3(300, 0, 300), 1));
   

    // 3. BAKE STATIC OBSTACLES (Trees/Rocks)
    // We iterate the environment obstacles once and mark them on the grid
    const std::vector<Obstacle>& envObstacles = environment->getObstacles();
    for (const auto& obs : envObstacles) {
        // Mark as BLOCKED (true)
        navGrid->updateArea(obs.position, obs.radius, true);
    }

    // 4. BAKE BUILDINGS
    // (If you have starting buildings)
    for (const auto& b : buildings) {
        float r = (b->getType() == BuildingType::TOWN_CENTER) ? 12.0f : 8.0f;
        navGrid->updateArea(b->getPosition(), r, true);
    }


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
            for (auto& u : units) if (u->isSelected()) workers.push_back(u.get());

            if (!workers.empty()) {
                auto& allObs = environment->getObstacles();
                for (auto& obs : allObs) {
                    if (!obs.active) continue;
                    bool inBox = false;
                    if (isClick) {
                        if (distance(obs.position, dragEndWorld) < obs.radius + 1.0f) inBox = true;
                    }
                    else {
                        vec2 sPos = worldToScreen(obs.position, V, P);
                        if (sPos.x >= minX && sPos.x <= maxX && sPos.y >= minY && sPos.y <= maxY) inBox = true;
                    }

                    if (inBox && !obs.marked) {
                        obs.marked = true;
                        for (auto* w : workers) w->assignGatherTask(obs.id);
                    }
                }
            }
            // Auto-switch back to normal mode after issuing command
            currentMode = InputMode::UNIT_SELECT;
        }
        // =========================================================
        // MODE C: ATTACK TARGETING (Red Box)
        // =========================================================
        else if (currentMode == InputMode::ATTACK_SELECT) {
            std::vector<Unit*> myUnits;
            for (auto& u : units) if (u->isSelected() && u->getTeam() == 0) myUnits.push_back(u.get());

            if (!myUnits.empty()) {
                std::vector<Unit*> enemyTargets;
                vec3 clickPos = dragEndWorld;

                // Find enemies in the box/click
                for (auto& u : units) {
                    if (u->getTeam() == 1) { // Is Enemy
                        bool targeted = false;
                        if (isClick) {
                            // Use larger radius (10.0f) for easier clicking
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
                    std::cout << "Command: ATTACK (" << enemyTargets.size() << " enemies queued)" << std::endl;

                    // SMART QUEUEING: Each soldier creates a sorted list of targets based on distance
                    for (auto* myUnit : myUnits) {

                        // Create a copy of the target list to sort for THIS specific unit
                        std::vector<Unit*> sortedTargets = enemyTargets;

                        // Sort: Closest enemies first
                        std::sort(sortedTargets.begin(), sortedTargets.end(),
                            [myUnit](Unit* a, Unit* b) {
                                float distA = glm::distance(myUnit->getPosition(), a->getPosition());
                                float distB = glm::distance(myUnit->getPosition(), b->getPosition());
                                return distA < distB;
                            });

                        // Send the sorted list to the unit's queue
                        myUnit->assignAttackQueue(sortedTargets);
                    }
                }
                else {
                    std::cout << "Attack command cancelled (No enemies clicked)" << std::endl;
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
    // Note: We use the same globals for drag start/end to simplify visuals, 
    // but logic handles them separately below.
    if (isRightDragging) dragEndWorld = getWorldPosUnderCursor();

    if (!isRightPressed && wasRightPressed && isRightDragging) {
        isRightDragging = false;
        vec3 clickPos = dragEndWorld;

        // Collect My Selected Units
        std::vector<Unit*> myUnits;
        for (auto& u : units) if (u->isSelected() && u->getTeam() == 0) myUnits.push_back(u.get());

        if (!myUnits.empty()) {
            // Right click is ALWAYS a move command (or cancel)
            std::cout << "Command: Move" << std::endl;
            for (auto* u : myUnits) u->clearTasks();

            // Unmark resources
            auto& allObs = environment->getObstacles();
            for (auto& o : allObs) o.marked = false;

            // Formation Move
            int count = myUnits.size();
            float spacing = 3.5f;
            for (int i = 0; i < count; i++) {
                float radius = spacing * std::sqrt(i + 1);
                float angle = i * 2.4f;
                vec3 offset(cos(angle) * radius, 0.0f, sin(angle) * radius);
                vec3 unitTarget = clickPos + offset;

                std::vector<vec3> path = Pathfinder::findPath(myUnits[i]->getPosition(), unitTarget, navGrid);
                myUnits[i]->setPath(path);
            }
            // If we were in attack mode, right click cancels it
            if (currentMode != InputMode::UNIT_SELECT) {
                currentMode = InputMode::UNIT_SELECT;
                std::cout << ">>> MODE: UNIT SELECTION (Cancelled) <<<" << std::endl;
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

    // ✅ COLOR BASED ON MODE
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
}

// Render: Selection Rings under units
void drawSelectionRings(mat4 V, mat4 P) {
    glDisable(GL_DEPTH_TEST);
    glUseProgram(0);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(&P[0][0]);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(&V[0][0]);

    for (const auto& u : units) {
        if (u->isSelected()) {
            vec3 pos = u->getPosition();

            // ✅ DYNAMIC COLOR LOGIC
            if (u->isGathering()) {
                glColor3f(1.0f, 1.0f, 0.0f); // Yellow (Working)
            }
            else if (u->isAttacking()) {
                glColor3f(1.0f, 0.0f, 0.0f); // Red (Fighting)
            }
            else {
                glColor3f(0.0f, 1.0f, 0.0f); // Green (Idle/Moving)
            }

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

    if (key1 && !last1) {
        placingBuilding = true;
        currentPlaceType = BuildingType::TOWN_CENTER;
        vec3 pos = getWorldPosUnderCursor();
        pos.y = 0.0f;
        previewBuilding = std::make_unique<Building>(currentPlaceType, pos);
    }
    if (key2 && !last2) {
        placingBuilding = true;
        currentPlaceType = BuildingType::BARRACKS;
        vec3 pos = getWorldPosUnderCursor();
        pos.y = 0.0f;
        previewBuilding = std::make_unique<Building>(currentPlaceType, pos);
    }
    if (key3 && !last3) {
        placingBuilding = true;
        currentPlaceType = BuildingType::SHOOTING_RANGE;
        vec3 pos = getWorldPosUnderCursor();
        pos.y = 0.0f;
        previewBuilding = std::make_unique<Building>(currentPlaceType, pos);
    }
    last1 = key1; last2 = key2; last3 = key3;

    if (placingBuilding && previewBuilding) {
        vec3 pos = getWorldPosUnderCursor();
        pos.y = 0.0f;
        previewBuilding->setPosition(pos);
    }

    static bool lastClick = false;
    bool clicked = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (clicked && !lastClick && placingBuilding) {
        ResourceCost cost = Building::getStaticCost(currentPlaceType);
        if (playerResources.spend(cost.wood, cost.rock)) {
            vec3 pos = getWorldPosUnderCursor();
            pos.y = 0.0f;
            float buildingRadius;
            if (currentPlaceType == BuildingType::TOWN_CENTER) buildingRadius = 15.0f;
            else if (currentPlaceType == BuildingType::BARRACKS) buildingRadius = 12.0f;
            else if (currentPlaceType == BuildingType::SHOOTING_RANGE) buildingRadius = 10.0f;
            else buildingRadius = 12.0f;

            terrain->flattenArea(pos, buildingRadius);
            buildings.push_back(std::make_unique<Building>(currentPlaceType, pos));

            if (navGrid) {
                navGrid->updateArea(pos, buildingRadius, true);
                std::cout << "Map updated: Blocked area at " << pos.x << ", " << pos.z << std::endl;
            }

            placingBuilding = false;
            previewBuilding = nullptr;
        }
        else {
            std::cout << "Not enough resources." << std::endl;
            placingBuilding = false;
            previewBuilding = nullptr;
        }
    }
    lastClick = clicked;
}

void drawBuildingUI()
{
    glDisable(GL_DEPTH_TEST);
    glUseProgram(0);

    mat4 P = camera->projectionMatrix;
    mat4 V = camera->viewMatrix;

    for (const auto& b : buildings) {
        vec3 buildingPos = b->getPosition();
        vec4 clipPos = P * V * vec4(buildingPos, 1.0f);
        if (clipPos.w <= 0.0f) continue;

        vec3 ndc = vec3(clipPos) / clipPos.w;
        float screenX = (ndc.x + 1.0f) * 0.5f * W_WIDTH;
        float screenY = (1.0f - ndc.y) * 0.5f * W_HEIGHT;

        if (screenX < -100 || screenX > W_WIDTH + 100 ||
            screenY < -100 || screenY > W_HEIGHT + 100) continue;

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, W_WIDTH, W_HEIGHT, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        const float radius = 40.0f;
        const float barWidth = 80.0f;
        const float barHeight = 8.0f;

        if (!b->isConstructed()) {
            float progress = b->getBuildProgress();
            glColor4f(0.2f, 0.2f, 0.2f, 0.7f);
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(screenX, screenY);
            for (int i = 0; i <= 32; ++i) {
                float angle = (i / 32.0f) * 2.0f * 3.14159f;
                glVertex2f(screenX + cos(angle) * radius, screenY + sin(angle) * radius);
            }
            glEnd();

            glColor4f(0.3f, 0.8f, 0.3f, 0.8f);
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(screenX, screenY);
            int segments = static_cast<int>(32 * progress);
            for (int i = 0; i <= segments; ++i) {
                float angle = (i / 32.0f) * 2.0f * 3.14159f - 3.14159f / 2.0f;
                glVertex2f(screenX + cos(angle) * radius, screenY + sin(angle) * radius);
            }
            glEnd();
        }

        if (b->isConstructed() && b->getCurrentHealth() < b->getMaxHealth()) {
            float healthPercent = b->getCurrentHealth() / b->getMaxHealth();
            float barY = screenY - 60.0f;

            glColor4f(0.3f, 0.0f, 0.0f, 0.7f);
            glBegin(GL_QUADS);
            glVertex2f(screenX - barWidth / 2, barY);
            glVertex2f(screenX + barWidth / 2, barY);
            glVertex2f(screenX + barWidth / 2, barY + barHeight);
            glVertex2f(screenX - barWidth / 2, barY + barHeight);
            glEnd();

            float r = 1.0f - healthPercent;
            float g = healthPercent;
            glColor4f(r, g, 0.0f, 0.9f);
            glBegin(GL_QUADS);
            glVertex2f(screenX - barWidth / 2, barY);
            glVertex2f(screenX - barWidth / 2 + barWidth * healthPercent, barY);
            glVertex2f(screenX - barWidth / 2 + barWidth * healthPercent, barY + barHeight);
            glVertex2f(screenX - barWidth / 2, barY + barHeight);
            glEnd();
        }

        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
    }
    glEnable(GL_DEPTH_TEST);
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
    glLineWidth(2.0f);
    const float len = 12.0f;
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

    // ✅ Use Compatibility Profile to support glBegin/glEnd logic
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_FALSE);

    window = glfwCreateWindow(W_WIDTH, W_HEIGHT, TITLE, nullptr, nullptr);
    if (!window) { glfwTerminate(); throw std::runtime_error("Window creation failed"); }
    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) throw std::runtime_error("GLEW init failed");
    glGetError(); // Clear false positive GLEW error

    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

    // ✅ MOUSE MUST BE NORMAL FOR RTS
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    camera = new Camera(window);
    glClearColor(0.5f, 0.5f, 0.5f, 0.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    logGLParameters();
}

void mainLoop() {
    float lastTime = static_cast<float>(glfwGetTime());
    const float cycleSpeed = 0.5f;

    static bool showDepthMap = false;
    static bool lastF1 = false;

    static float unitCameraAngle = 3.14159f; // Start facing unit back

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
            cos(angle) * sin(tiltAngle)
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
        for (auto& b : buildings) {
            b->updateConstruction(dt);
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

        glUseProgram(shadowShader);
        glUniformMatrix4fv(glGetUniformLocation(shadowShader, "lightSpaceMatrix"), 1, GL_FALSE, &lightSpaceMatrix[0][0]);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(0.5f, 0.5f);

        mat4 model = mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shadowShader, "model"), 1, GL_FALSE, &model[0][0]);
        terrain->draw(model, shadowShader);

        GLint modelLocShadow = glGetUniformLocation(shadowShader, "model");
        environment->draw(shadowShader, modelLocShadow);

        for (const auto& b : buildings) {
            mat4 model = translate(mat4(1.0f), b->getPosition());
            float scale = (b->getType() == BuildingType::TOWN_CENTER) ? 10.0f : ((b->getType() == BuildingType::BARRACKS) ? 7.0f : 5.0f);
            model = glm::scale(model, vec3(scale));
            glUniformMatrix4fv(glGetUniformLocation(shadowShader, "model"), 1, GL_FALSE, &model[0][0]);
            b->getMesh()->draw();
        }

        for (const auto& u : units) {
            u->draw(lightView, lightProjection, shadowShader, glfwGetTime());
        }

        glDisable(GL_POLYGON_OFFSET_FILL);
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

        // B. Standard Objects
        glUseProgram(shaderProgram);
        glActiveTexture(GL_TEXTURE0);
        glUniformMatrix4fv(projectionMatrixLocation, 1, GL_FALSE, &P[0][0]);
        glUniformMatrix4fv(viewMatrixLocation, 1, GL_FALSE, &V[0][0]);
        glUniform3f(lightDirectionLocation, light.dir.x, light.dir.y, light.dir.z);
        glUniform1f(lightPowerLocation, light.power);
        glUniform4fv(LaLocation, 1, &light.La[0]);
        glUniform4fv(LdLocation, 1, &light.Ld[0]);
        glUniform4fv(LsLocation, 1, &light.Ls[0]);
        glUniformMatrix4fv(lightSpaceMatrixLoc_standard, 1, GL_FALSE, &lightSpaceMatrix[0][0]);
        glUniform1i(shadowMapLoc_standard, 5);

        glUniform1f(glGetUniformLocation(shaderProgram, "constructionProgress"), 1.0f);
        glUniform1f(glGetUniformLocation(shaderProgram, "buildingHeight"), 50.0f);
        glUniform3f(glGetUniformLocation(shaderProgram, "buildingBasePos"), 0, 0, 0);

        uploadMaterial(defaultMaterial);
        environment->draw(shaderProgram, modelMatrixLocation);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);
        for (const auto& b : buildings) {
            b->draw(V, P, shaderProgram, 1.0f);
        }
        if (previewBuilding) {
            previewBuilding->draw(V, P, shaderProgram, 0.3f);
        }
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);

        // C. Skinned Units
        glUseProgram(skinnedShader);
        glUniformMatrix4fv(glGetUniformLocation(skinnedShader, "P"), 1, GL_FALSE, &P[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(skinnedShader, "V"), 1, GL_FALSE, &V[0][0]);
        glUniform3f(glGetUniformLocation(skinnedShader, "lightDirection_worldspace"), light.dir.x, light.dir.y, light.dir.z);
        glUniform1f(glGetUniformLocation(skinnedShader, "light.power"), light.power);
        glUniform4fv(glGetUniformLocation(skinnedShader, "light.La"), 1, &light.La[0]);
        glUniform4fv(glGetUniformLocation(skinnedShader, "light.Ld"), 1, &light.Ld[0]);
        glUniform4fv(glGetUniformLocation(skinnedShader, "light.Ls"), 1, &light.Ls[0]);
        glUniformMatrix4fv(glGetUniformLocation(skinnedShader, "lightSpaceMatrix"), 1, GL_FALSE, &lightSpaceMatrix[0][0]);
        glUniform1i(glGetUniformLocation(skinnedShader, "shadowMap"), 5);
        glUniform1f(glGetUniformLocation(skinnedShader, "constructionProgress"), 1.0f);
        glUniform1f(glGetUniformLocation(skinnedShader, "buildingHeight"), 10.0f);
        glUniform3f(glGetUniformLocation(skinnedShader, "buildingBasePos"), 0, 0, 0);
        glUniform4f(glGetUniformLocation(skinnedShader, "mtl.Ka"), 0.2f, 0.2f, 0.2f, 1.0f);
        glUniform4f(glGetUniformLocation(skinnedShader, "mtl.Kd"), 1.0f, 1.0f, 1.0f, 1.0f);
        glUniform4f(glGetUniformLocation(skinnedShader, "mtl.Ks"), 0.5f, 0.5f, 0.5f, 1.0f);
        glUniform1f(glGetUniformLocation(skinnedShader, "mtl.Ns"), 50.0f);

        for (const auto& u : units) {
            u->draw(V, P, skinnedShader, glfwGetTime());
        }
        glUseProgram(shaderProgram);

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
    units.clear();         // <--- ADD THIS
    buildings.clear();

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

    delete shadowMap; shadowMap = nullptr;
    delete snowTrailMap; snowTrailMap = nullptr;

    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteBuffers(1, &quadEBO);
    glDeleteVertexArrays(1, &dummyPointVAO);
    glDeleteBuffers(1, &dummyPointVBO);

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
        /*std::cin.get();*/
        freeResources();
        return -1;
    }
    return 0;
}