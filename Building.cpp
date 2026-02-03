#include "Building.h"
#include "Mesh.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include "Unit.h" // Need this to know about UnitType enum

Building::Building(BuildingType type, const glm::vec3& pos, int teamID, const std::string& meshPath)
    : type_(type), position_(pos), teamID_(teamID), mesh_(nullptr),
    currentHealth_(100.0f), buildProgress_(0.0f), isConstructed_(false)
{
    // ✅ Initialize stats first
    initializeStats();

    std::string path = meshPath;

    // Choose model based on building type
    if (path.empty()) {
        switch (type) {
        case BuildingType::TOWN_CENTER:
            path = "models/building_castle_blue.obj";
            break;
        case BuildingType::BARRACKS:
            path = "models/building_barracks_blue.obj";
            break;
        case BuildingType::SHOOTING_RANGE:
            path = "models/building_archeryrange_blue.obj";
            break;
        default:
            path = "models/cube.obj";
            break;
        }
    }

    std::cout << "Loading building model: '" << path << "'" << std::endl;

    // Test if file exists
    std::ifstream test(path);
    if (!test.is_open()) {
        std::cerr << "WARNING: Cannot open '" << path << "' → falling back to cube.obj\n";
        path = "models/cube.obj";
    }
    test.close();

    try {
        mesh_ = new Mesh(path);
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR loading mesh: " << e.what() << " → using fallback cube.obj\n";
        if (path != "models/cube.obj") {
            delete mesh_;
            mesh_ = new Mesh("models/cube.obj");
        }
        else {
            throw;
        }
    }

    // ✅ Calculate building dimensions and base position
    float scale;
    if (type_ == BuildingType::TOWN_CENTER) {
        scale = 1.0f;
        buildingHeight_ = scale * 1.0f;  // Cube/mesh height when scaled
    }
    else if (type_ == BuildingType::BARRACKS) {
        scale = 1.0f;
        buildingHeight_ = scale * 1.0f;
    }
    else if (type_ == BuildingType::SHOOTING_RANGE) {
        scale = 1.0f;
        buildingHeight_ = scale * 1.0f;
    }
    else {
        scale = 1.0f;
        buildingHeight_ = scale * 1.0f;
    }

    // ✅ Store base position (bottom of building on ground)
    basePosition_ = pos;
    basePosition_.y = pos.y;  // Ground level

    // Center position (for model matrix)
    position_.y += scale * 0.5f;
}

Building::~Building()
{
    delete mesh_;
    mesh_ = nullptr;
}

// ✅ Initialize building stats based on type
void Building::initializeStats()
{
    switch (type_) {
    case BuildingType::TOWN_CENTER:
        stats_.maxHealth = 2000.0f;
        stats_.buildTime = 60.0f;
        stats_.buildCost = { 200, 100 };
        stats_.repairCost = { 1, 0 };
        break;

    case BuildingType::BARRACKS:
        stats_.maxHealth = 1500.0f;
        stats_.buildTime = 45.0f;
        stats_.buildCost = { 150, 50 };
        stats_.repairCost = { 1, 0 };
        break;

    case BuildingType::SHOOTING_RANGE:
        stats_.maxHealth = 1000.0f;
        stats_.buildTime = 30.0f;
        stats_.buildCost = { 100, 75 };
        stats_.repairCost = { 1, 0 };
        break;

    default:
        stats_.maxHealth = 1000.0f;
        stats_.buildTime = 30.0f;
        stats_.buildCost = { 100, 50 };
        stats_.repairCost = { 1, 0 };
        break;
    }

    currentHealth_ = stats_.maxHealth;
}

void Building::draw(const glm::mat4& view, const glm::mat4& projection,
    GLuint shaderProgram, float passedAlpha, glm::vec3 tint)
{
    // -----------------------------------------------------------
    // 1. SETUP (Scale & Matrix)
    // -----------------------------------------------------------
    float scale = 1.0f;
    if (type_ == BuildingType::TOWN_CENTER) scale = 10.0f;
    else if (type_ == BuildingType::BARRACKS) scale = 10.0f;
    else scale = 10.0f;

    glm::mat4 model = glm::mat4(1.0f);

    // 1. Move to World Position
    model = glm::translate(model, basePosition_);

    // 2. ✅ ROTATE 180 DEGREES (Add this line)
    model = glm::rotate(model, glm::radians(160.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // 3. Sit on ground offset (if needed)
    model = glm::translate(model, glm::vec3(0.0f, 0.0f * scale, 0.0f));

    // 4. Apply Scale
    model = glm::scale(model, glm::vec3(scale));

    // Send Matrices
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "M"), 1, GL_FALSE, &model[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "V"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "P"), 1, GL_FALSE, &projection[0][0]);

    // -----------------------------------------------------------
    // 2. DETERMINE VISUAL STATE
    // -----------------------------------------------------------
    float finalAlpha = 1.0f;
    float clipProgress = 1.0f;

    // CASE A: PREVIEW MODE
    // If we passed a transparency value (like 0.3), treat it as a preview.
    // Show the whole building (clip = 1.0), but transparent.
    if (passedAlpha < 0.99f) {
        finalAlpha = 0.3f; // 70% transparent
        clipProgress = 1.0f; // No clipping, show full ghost
    }
    // CASE B: UNDER CONSTRUCTION
    else if (!isConstructed_) {
        // Fade from 50% (0.5) to 100% (1.0) based on progress
        finalAlpha = 0.5f + (0.5f * buildProgress_);
        
        // Grow from bottom up
        clipProgress = buildProgress_;
    }
    // CASE C: COMPLETED BUILDING
    else {
        finalAlpha = 1.0f; // Solid
        clipProgress = 1.0f; // Full height
    }

    // -----------------------------------------------------------
    // 3. SEND UNIFORMS
    // -----------------------------------------------------------

    // Send Transparency & Color
    GLuint kdLoc = glGetUniformLocation(shaderProgram, "mtl.Kd");
    if (kdLoc != -1) {
        // ✅ USE THE TINT COLOR HERE
        glUniform4f(kdLoc, tint.r, tint.g, tint.b, finalAlpha);
    }

    // ... (Keep the rest of uniform sending & mesh drawing) ...
    float worldHeight = (2.0f * scale) + 50.0f;
    glm::vec3 basePos = basePosition_;

    glUniform1f(glGetUniformLocation(shaderProgram, "constructionProgress"), clipProgress);
    glUniform1f(glGetUniformLocation(shaderProgram, "buildingHeight"), worldHeight);
    glUniform3fv(glGetUniformLocation(shaderProgram, "buildingBasePos"), 1, &basePos[0]);

    if (mesh_) mesh_->draw();
}

void Building::setPosition(const glm::vec3& pos)
{
    basePosition_ = pos;
    basePosition_.y = 0.0f; // Ensure it stays on ground

    // Update position_ for other logic, but 'draw' relies on basePosition_ now.
    position_ = basePosition_;
}

// ✅ Update building construction over time
void Building::updateConstruction(float deltaTime)
{
    if (isConstructed_) return;

    buildProgress_ += deltaTime / stats_.buildTime;

    if (buildProgress_ >= 1.0f) {
        buildProgress_ = 1.0f;
        isConstructed_ = true;
        currentHealth_ = stats_.maxHealth;
        std::cout << "Building construction complete!" << std::endl;
    }
}

// ✅ Take damage
void Building::takeDamage(float damage)
{
    if (!isConstructed_) return;

    currentHealth_ -= damage;
    if (currentHealth_ < 0.0f) {
        currentHealth_ = 0.0f;
        std::cout << "Building destroyed!" << std::endl;
    }
}

// ✅ Repair building
void Building::repair(float amount)
{
    if (!isConstructed_) return;

    currentHealth_ += amount;
    if (currentHealth_ > stats_.maxHealth) {
        currentHealth_ = stats_.maxHealth;
    }
}

ResourceCost Building::getStaticCost(BuildingType type) {
    // Return the hardcoded values here so you don't need an object instance
    switch (type) {
    case BuildingType::TOWN_CENTER: return { 200, 100 };
    case BuildingType::BARRACKS:    return { 150, 50 };
    case BuildingType::SHOOTING_RANGE: return { 100, 75 };
    default: return { 0, 0 };
    }
}

// ----------------------------------------------------------------------
// AUTO SPAWNING LOGIC
// ----------------------------------------------------------------------

UnitType Building::updateAutoSpawning(float dt)
{
    // 1. Only spawn if fully constructed, alive, AND below cap
    if (!isConstructed_ || currentHealth_ <= 0.0f) return (UnitType)-1;

    if (spawnedCount_ >= MAX_UNIT_CAP) return (UnitType)-1; // ✅ STOP if cap reached

    // 2. Advance Timer
    autoSpawnTimer_ += dt;

    // 3. Trigger Spawn
    if (autoSpawnTimer_ >= SPAWN_INTERVAL) {
        autoSpawnTimer_ = 0.0f;

        // ✅ Increment Counter
        spawnedCount_++;

        switch (type_) {
        case BuildingType::TOWN_CENTER: return UnitType::WORKER;
        case BuildingType::BARRACKS:    return UnitType::MELEE;
        case BuildingType::SHOOTING_RANGE: return UnitType::RANGED;
        default: return (UnitType)-1;
        }
    }

    return (UnitType)-1;
}

// ✅ Get building height (for clipping calculations)
float Building::getBuildingHeight() const
{
    return buildingHeight_;
}