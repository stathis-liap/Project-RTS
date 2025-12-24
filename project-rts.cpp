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
#include "Terrain.h"
#include "ShadowMap.h"
#include "Mesh.h"
#include "SnowTrailMap.h"
#include "Building.h"
#include <vector>
#include <memory>
#include "Unit.h"
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
GLuint dummyPointVAO = 0, dummyPointVBO = 0; // ← NEW: for snow trail points
GLuint shadowMapTexUnit = 5;
bool showDepthMap = false;
GLuint shadowMapLoc_standard = 0;
GLuint lightSpaceMatrixLoc_standard = 0;

GLuint whiteShader = 0;

// Uniform locations (standard shader)
GLuint projectionMatrixLocation, viewMatrixLocation, modelMatrixLocation;
GLuint KaLocation, KdLocation, KsLocation, NsLocation;
GLuint LaLocation, LdLocation, LsLocation, lightDirectionLocation, lightPowerLocation;
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
// ---------------------------------------------------------------
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
// Handle building placement
void updateBuildingPlacement()
{
    static bool last1 = false, last2 = false;
    bool key1 = glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;
    bool key2 = glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS;
    if (key1 && !last1) {
        placingBuilding = true;
        currentPlaceType = BuildingType::TOWN_CENTER;
    }
    if (key2 && !last2) {
        placingBuilding = true;
        currentPlaceType = BuildingType::BARRACKS;
    }
    last1 = key1; last2 = key2;
    if (placingBuilding) {
        vec3 pos = getWorldPosUnderCursor();
        pos.y = 0.0f;
        previewBuilding = std::make_unique<Building>(currentPlaceType, pos);
    }
    else {
        previewBuilding = nullptr;
    }
    static bool lastClick = false;
    bool clicked = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (clicked && !lastClick && placingBuilding) {
        vec3 pos = getWorldPosUnderCursor();
        pos.y = 0.0f;
        buildings.push_back(std::make_unique<Building>(currentPlaceType, pos));
        placingBuilding = false;
        previewBuilding = nullptr;
    }
    lastClick = clicked;
}
// 2-D CROSSHAIR
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
// ---------------------------------------------------------------
void createContext()
{
    // Standard shader
    shaderProgram = loadShaders("StandardShading.vertexshader", "StandardShading.fragmentshader");
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

    shadowShader = loadShaders("ShadowMap.vertexshader", "ShadowMap.fragmentshader");
    shadowMap = new ShadowMap(2048, 2048);

    snowTrailMap = new SnowTrailMap(1024, 1024);
    snowTrailMap->clear(); // ← FULL SNOW at start

    snowTrailShader = loadShaders("SnowTrail.vertexshader", "SnowTrail.fragmentshader");

    debugShader = loadShaders("debugDepthShader.vertexshader", "debugDepthShader.fragmentshader");

    whiteShader = loadShaders("White.vertexshader", "White.fragmentshader");

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
    buildings.push_back(std::make_unique<Building>(BuildingType::TOWN_CENTER, vec3(256.0f, 0.0f, 256.0f)));
    units.push_back(std::make_unique<Unit>(UnitType::WORKER, vec3(260.0f, 0.0f, 260.0f)));
    units.push_back(std::make_unique<Unit>(UnitType::MELEE, vec3(250.0f, 0.0f, 270.0f)));
    units.push_back(std::make_unique<Unit>(UnitType::RANGED, vec3(270.0f, 0.0f, 250.0f)));
}
// ---------------------------------------------------------------
void initialize()
{
    if (!glfwInit()) throw std::runtime_error("GLFW init failed");
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(W_WIDTH, W_HEIGHT, TITLE, nullptr, nullptr);
    if (!window) { glfwTerminate(); throw std::runtime_error("Window creation failed"); }
    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) throw std::runtime_error("GLEW init failed");
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    camera = new Camera(window);
    glClearColor(0.5f, 0.5f, 0.5f, 0.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    logGLParameters();
}
// ---------------------------------------------------------------
void mainLoop() {
    float lastTime = static_cast<float>(glfwGetTime());
    const float cycleSpeed = 0.5f; // full day in ~2 minutes

    static bool showDepthMap = false;
    static bool lastF1 = false;

    do {
        float currentTime = static_cast<float>(glfwGetTime());
        float dt = currentTime - lastTime;
        lastTime = currentTime;

        // -------------------------------------------------------
        // 1. UPDATE SUN DIRECTION & INTENSITY
        // -------------------------------------------------------
        float angle = currentTime * cycleSpeed + radians(90.0f);
        light.dir = normalize(vec3(cos(angle), sin(angle), 0.0f)); // TOWARD light (up at noon)

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

        // -------------------------------------------------------
        // 2. SKY COLOR
        // -------------------------------------------------------
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
        // 3. UPDATE CAMERA
        // -------------------------------------------------------
        camera->update();
        const mat4 P = camera->projectionMatrix;
        const mat4 V = camera->viewMatrix;

        // -------------------------------------------------------
        // 4. UPDATE GAME LOGIC
        // -------------------------------------------------------
        updateBuildingPlacement();

        for (auto& u : units) {
            u->update(dt);
        }

        // -------------------------------------------------------
        // 5. SHADOW MAP PASS
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

        // ✅ Enable depth testing
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        glClear(GL_DEPTH_BUFFER_BIT);

        glUseProgram(shadowShader);
        glUniformMatrix4fv(glGetUniformLocation(shadowShader, "lightSpaceMatrix"), 1, GL_FALSE, &lightSpaceMatrix[0][0]);

        // ✅ Use polygon offset instead of front-face culling
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.5f, 2.0f);  // ✅ Moderate offset that works with smaller bias

        // Terrain
        {
            mat4 model = mat4(1.0f);
            glUniformMatrix4fv(glGetUniformLocation(shadowShader, "model"), 1, GL_FALSE, &model[0][0]);
            terrain->draw(model, shadowShader);
        }

        // Buildings
        for (const auto& b : buildings) {
            mat4 model = translate(mat4(1.0f), b->getPosition());
            float scale = (b->getType() == BuildingType::TOWN_CENTER) ? 11.0f : 8.0f;
            model = glm::scale(model, vec3(scale));
            glUniformMatrix4fv(glGetUniformLocation(shadowShader, "model"), 1, GL_FALSE, &model[0][0]);
            b->getMesh()->draw();
        }

        // Units
        for (const auto& u : units) {
            mat4 model = translate(mat4(1.0f), u->getPosition());
            model = glm::scale(model, vec3(1.5f));
            glUniformMatrix4fv(glGetUniformLocation(shadowShader, "model"), 1, GL_FALSE, &model[0][0]);
            u->getMesh()->draw();
        }

        glDisable(GL_POLYGON_OFFSET_FILL);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, W_WIDTH, W_HEIGHT);

        // -------------------------------------------------------
        // 6. SNOW TRAIL PASS – Persistent trails with regeneration
        // -------------------------------------------------------
        snowTrailMap->bindForWriting();

        glDisable(GL_DEPTH_TEST);

        // ✅ STEP 1: Brighten entire texture (read, add 0.001, write back)
        glDisable(GL_BLEND);  // No blending - direct write

        glUseProgram(whiteShader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, snowTrailMap->getTexture());
        glUniform1i(glGetUniformLocation(whiteShader, "currentSnow"), 0);

        glBindVertexArray(quadVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // ✅ STEP 2: Darken where units walk
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
        // 7. DEBUG F1
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
        // 8. MAIN RENDER PASS
        // -------------------------------------------------------
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, shadowMap->getDepthTexture());
        glActiveTexture(GL_TEXTURE6);
        snowTrailMap->bindForReading(GL_TEXTURE6);

        // Terrain
        glUseProgram(terrainShader);
        glUniformMatrix4fv(glGetUniformLocation(terrainShader, "P"), 1, GL_FALSE, &P[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(terrainShader, "V"), 1, GL_FALSE, &V[0][0]);
        mat4 terrainM = mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(terrainShader, "M"), 1, GL_FALSE, &terrainM[0][0]);

        // FLIPPED direction for shading
        glUniform3f(glGetUniformLocation(terrainShader, "lightDirection_worldspace"),
            light.dir.x, light.dir.y, light.dir.z);
        glUniform1f(glGetUniformLocation(terrainShader, "lightPower"), light.power);

        glUniformMatrix4fv(glGetUniformLocation(terrainShader, "lightSpaceMatrix"), 1, GL_FALSE, &lightSpaceMatrix[0][0]);
        glUniform1i(glGetUniformLocation(terrainShader, "shadowMap"), 5);
        glUniform1i(glGetUniformLocation(terrainShader, "snowTrailMap"), 6);

        terrain->draw(terrainM);

        // Buildings & Units
        glUseProgram(shaderProgram);
        glUniformMatrix4fv(projectionMatrixLocation, 1, GL_FALSE, &P[0][0]);
        glUniformMatrix4fv(viewMatrixLocation, 1, GL_FALSE, &V[0][0]);

        // FLIPPED direction for shading
        glUniform3f(lightDirectionLocation, light.dir.x, light.dir.y, light.dir.z);
        glUniform1f(lightPowerLocation, light.power);
        glUniform4fv(LaLocation, 1, &light.La[0]);
        glUniform4fv(LdLocation, 1, &light.Ld[0]);
        glUniform4fv(LsLocation, 1, &light.Ls[0]);

        uploadMaterial(defaultMaterial);

        glUniformMatrix4fv(lightSpaceMatrixLoc_standard, 1, GL_FALSE, &lightSpaceMatrix[0][0]);
        glUniform1i(shadowMapLoc_standard, 5);

        for (const auto& b : buildings) b->draw(V, P, shaderProgram);

        if (previewBuilding) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            previewBuilding->draw(V, P, shaderProgram);
            glDisable(GL_BLEND);
        }

        for (const auto& u : units) u->draw(V, P, shaderProgram);

        drawCursor();

        glfwSwapBuffers(window);
        glfwPollEvents();

    } while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
        glfwWindowShouldClose(window) == 0);
}
// ---------------------------------------------------------------
void freeResources()
{
    delete terrain; terrain = nullptr;
    delete camera; camera = nullptr;
    glDeleteProgram(shaderProgram);
    glDeleteProgram(terrainShader);
    delete shadowMap; shadowMap = nullptr;
    glDeleteProgram(shadowShader);
    glDeleteProgram(debugShader);
    glDeleteProgram(snowTrailShader);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteBuffers(1, &quadEBO);
    glDeleteVertexArrays(1, &dummyPointVAO);
    glDeleteBuffers(1, &dummyPointVBO);
    delete snowTrailMap; snowTrailMap = nullptr;
    glDeleteProgram(whiteShader);
    buildings.clear();
    glfwTerminate();
}
// ---------------------------------------------------------------
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