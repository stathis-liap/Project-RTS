// ---------------------------------------------------------------
//  main.cpp  (only the parts you need to change/add are marked)
// ---------------------------------------------------------------
#include <iostream>
#include <string>
#include <algorithm>                 // for std::max / std::min
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GL/glew.h>
#include <glfw3.h>

#include <common/shader.h>
#include <common/util.h>
#include <common/camera.h>          // <-- your RTS camera
#include "Terrain.h"

// ---------------------------------------------------------------

using namespace std;
using namespace glm;

// ---------------------------------------------------------------

#define W_WIDTH  1920
#define W_HEIGHT  1080
#define TITLE    "RTS – Terrain + Cursor + Zoom-to-cursor"

// ---------------------------------------------------------------

GLFWwindow* window = nullptr;
Camera* camera = nullptr;
GLuint  shaderProgram = 0;
GLuint  terrainShader = 0;

// Uniform locations (standard shader)
GLuint projectionMatrixLocation, viewMatrixLocation, modelMatrixLocation;
GLuint KaLocation, KdLocation, KsLocation, NsLocation;
GLuint LaLocation, LdLocation, LsLocation, lightDirectionLocation, lightPowerLocation; 

// ---------------------------------------------------------------

struct Light {
    vec4 La, Ld, Ls;
    vec3 dir;  // <-- NEW: Direction (normalized, pointing TOWARDS light)
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
    vec4{0.2f,0.2f,0.2f,1},   // ambient (slight blue tint at night)
    vec4{1,1,1,1},            // diffuse (changes during cycle)
    vec4{1,1,1,1},            // specular
    vec3(0,10,0),              // initial dir (straight up = noon)
    1.0f                      // power (scales during cycle)
};

Terrain* terrain = nullptr;

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
    glUniform3f(lightDirectionLocation, l.dir.x, l.dir.y, l.dir.z);  // <-- direction
    glUniform1f(lightPowerLocation, l.power);
}

// ---------------------------------------------------------------

// ---------------------------------------------------------------
//  2-D CROSSHAIR (immediate mode – works with core profile)
// ---------------------------------------------------------------
void drawCursor()
{
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);

    // ---- switch to orthographic 2-D screen space ----------------
    glDisable(GL_DEPTH_TEST);
    glUseProgram(0);                     // no shader needed
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, W_WIDTH, W_HEIGHT, 0, -1, 1);   // Y-up screen
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

    // ---- restore 3-D state ------------------------------------
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glEnable(GL_DEPTH_TEST);
}

// ---------------------------------------------------------------

void createContext()
{
    // ---- standard shader (for any objects you add later) ----------
    shaderProgram = loadShaders(
        "StandardShading.vertexshader",
        "StandardShading.fragmentshader");

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

    // FIXED: Use correct uniform name from shader
    lightDirectionLocation = glGetUniformLocation(shaderProgram, "lightDirection_worldspace");
    lightPowerLocation = glGetUniformLocation(shaderProgram, "light.power");

    // ---- terrain shader -------------------------------------------
    terrainShader = loadShaders("terrain.vertexshader", "terrain.fragmentshader");

    // ---- terrain object -------------------------------------------
    terrain = new Terrain(512, 512, 1.1f);
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

    // ---- INPUT ----------------------------------------------------
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    // **IMPORTANT** – keep the cursor visible & free
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // ---- CAMERA ---------------------------------------------------
    camera = new Camera(window);               // <-- our RTS camera
    // (the scroll callback is already registered inside Camera ctor)

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
    const float cycleSpeed = 0.005f; // full day in ~2 minutes

    do {
        float currentTime = static_cast<float>(glfwGetTime());
        float dt = currentTime - lastTime;
        lastTime = currentTime;

        // -------------------------------------------------------
        // 1. UPDATE SUN DIRECTION & INTENSITY (Day/Night Cycle)
        // -------------------------------------------------------
        float angle = currentTime * cycleSpeed;
        light.dir = normalize(vec3(sin(angle), cos(angle), sin(angle * 0.3f))); // 3D orbit

        if (light.dir.y > 0.0f) { // DAY
            light.power = 1.0f;
            light.Ld = mix(
                vec4(1.0f, 0.8f, 0.6f, 1.0f), // sunrise
                vec4(1.0f, 1.0f, 0.9f, 1.0f), // midday
                light.dir.y
            );
            light.La = vec4(0.25f, 0.3f, 0.4f, 1.0f); // bright ambient
        }
        else { // NIGHT
            light.power = 0.15f;
            light.Ld = vec4(0.4f, 0.4f, 0.7f, 1.0f); // moonlight
            light.La = vec4(0.05f, 0.05f, 0.1f, 1.0f); // dark ambient
        }

        // -------------------------------------------------------
        // 2. SKY COLOR (matches sun)
        // -------------------------------------------------------
        vec3 skyDay = vec3(0.53f, 0.81f, 0.92f);
        vec3 skyNight = vec3(0.01f, 0.01f, 0.05f);
        vec3 skyColor = mix(skyNight, skyDay, glm::max(light.dir.y, 0.0f));
        glClearColor(skyColor.r, skyColor.g, skyColor.b, 1.0f);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // -------------------------------------------------------
        // 3. UPDATE CAMERA
        // -------------------------------------------------------
        camera->update();
        const mat4 P = camera->projectionMatrix;
        const mat4 V = camera->viewMatrix;

        // -------------------------------------------------------
        // 4. RENDER STANDARD OBJECTS (if any)
        // -------------------------------------------------------
        glUseProgram(shaderProgram);
        glUniformMatrix4fv(projectionMatrixLocation, 1, GL_FALSE, &P[0][0]);
        glUniformMatrix4fv(viewMatrixLocation, 1, GL_FALSE, &V[0][0]);
        uploadLight(light);
        uploadMaterial(defaultMaterial);

        // -------------------------------------------------------
        // 5. RENDER TERRAIN
        // -------------------------------------------------------
        glUseProgram(terrainShader);

        // --- Matrices ---
        GLuint tP = glGetUniformLocation(terrainShader, "P");
        GLuint tV = glGetUniformLocation(terrainShader, "V");
        GLuint tM = glGetUniformLocation(terrainShader, "M");
        glUniformMatrix4fv(tP, 1, GL_FALSE, &P[0][0]);
        glUniformMatrix4fv(tV, 1, GL_FALSE, &V[0][0]);
        mat4 terrainM = mat4(1.0f);
        glUniformMatrix4fv(tM, 1, GL_FALSE, &terrainM[0][0]);

        // --- LIGHT: Upload direction + power to terrain shader ---
        GLuint terrainLightDirLoc = glGetUniformLocation(terrainShader, "lightDirection_worldspace");
        GLuint terrainLightPowerLoc = glGetUniformLocation(terrainShader, "lightPower");
        glUniform3f(terrainLightDirLoc, light.dir.x, light.dir.y, light.dir.z);
        glUniform1f(terrainLightPowerLoc, light.power);

        // --- Material ---
        uploadMaterial(defaultMaterial);

        terrain->draw(terrainM);

        // -------------------------------------------------------
        // 6. DRAW CURSOR
        // -------------------------------------------------------
        drawCursor();

        glfwSwapBuffers(window);
        glfwPollEvents();

    } while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
        glfwWindowShouldClose(window) == 0);
}

// ---------------------------------------------------------------

void freeResources()
{
    delete terrain;  terrain = nullptr;
    delete camera;   camera = nullptr;
    glDeleteProgram(shaderProgram);
    glDeleteProgram(terrainShader);
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