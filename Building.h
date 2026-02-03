// ============================================================
// Building.h
// ============================================================
#pragma once
#include <glm/glm.hpp>
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

class Mesh;

// ✅ CRITICAL FIX: Add this line here!
// This tells the compiler "UnitType exists" so it can parse the function below.
enum class UnitType;

enum class BuildingType {
    TOWN_CENTER,
    BARRACKS,
    SHOOTING_RANGE
};

struct ResourceCost {
    int wood;
    int rock;
};

struct BuildingStats {
    float maxHealth;
    float buildTime;
    ResourceCost buildCost;
    ResourceCost repairCost;
};

class Building {
public:
    // Constructor
    Building(BuildingType type, const glm::vec3& pos, int teamID, const std::string& meshPath = "");
    ~Building();

    // In Building.h
    void draw(const glm::mat4& view, const glm::mat4& projection,
        GLuint shaderProgram, float passedAlpha,
        glm::vec3 tint = glm::vec3(0.8f, 0.8f, 0.8f)); 

    float getBuildingHeight() const;
    glm::vec3 getBasePosition() const { return basePosition_; }

    // Getters
    BuildingType getType() const { return type_; }
    glm::vec3 getPosition() const { return position_; }
    Mesh* getMesh() const { return mesh_; }

    // Stats
    float getCurrentHealth() const { return currentHealth_; }
    float getMaxHealth() const { return stats_.maxHealth; }
    float getBuildProgress() const { return buildProgress_; }
    bool isConstructed() const { return isConstructed_; }
    const BuildingStats& getStats() const { return stats_; }

    // Team Logic
    int getTeam() const { return teamID_; }
    bool isDead() const { return currentHealth_ <= 0; }

    // ✅ Now the compiler knows 'UnitType' is a valid type
    UnitType updateAutoSpawning(float dt);

    // ✅ NEW: Cap Logic
    int getSpawnedCount() const { return spawnedCount_; }
    bool isCapReached() const { return spawnedCount_ >= MAX_UNIT_CAP; }

    void setPosition(const glm::vec3& pos);

    void updateConstruction(float deltaTime);
    void takeDamage(float damage);
    void repair(float amount);

    ResourceCost getBuildCost() const { return stats_.buildCost; }
    ResourceCost getRepairCost() const { return stats_.repairCost; }

    static ResourceCost getStaticCost(BuildingType type);

private:
    BuildingType type_;
    glm::vec3 position_;
    Mesh* mesh_;

    int teamID_;

    BuildingStats stats_;
    float currentHealth_;
    float buildProgress_;
    bool isConstructed_;

    glm::vec3 basePosition_;
    float buildingHeight_;

    float autoSpawnTimer_ = 0.0f;
    const float SPAWN_INTERVAL = 2.0f; // Seconds between units

    // ✅ NEW: Unit Cap Variables
    int spawnedCount_ = 0;
    const int MAX_UNIT_CAP = 10;

    void initializeStats();
};