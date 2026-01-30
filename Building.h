// ============================================================
// Building.h - Enhanced with stats and resources
// ============================================================
#pragma once
#include <glm/glm.hpp>
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

class Mesh;

// ✅ Building types including new SHOOTING_RANGE
enum class BuildingType {
    TOWN_CENTER,
    BARRACKS,
    SHOOTING_RANGE  // ✅ New building type
};

// ✅ Resource costs structure
struct ResourceCost {
    int wood;
    int rock;
};

// ✅ Building stats structure
struct BuildingStats {
    float maxHealth;
    float buildTime;        // seconds to build
    ResourceCost buildCost;
    ResourceCost repairCost; // cost per health point
};

class Building {
public:
    Building(BuildingType type, const glm::vec3& pos, const std::string& meshPath = "");
    ~Building();

    void draw(const glm::mat4& view, const glm::mat4& projection,
        GLuint shaderProgram, float alpha = 1.0f);

    float getBuildingHeight() const;  // ✅ NEW
    glm::vec3 getBasePosition() const { return basePosition_; }  // ✅ NEW



    // ✅ Getters
    BuildingType getType() const { return type_; }
    glm::vec3 getPosition() const { return position_; }
    Mesh* getMesh() const { return mesh_; }

    // ✅ New getters for stats
    float getCurrentHealth() const { return currentHealth_; }
    float getMaxHealth() const { return stats_.maxHealth; }
    float getBuildProgress() const { return buildProgress_; }
    bool isConstructed() const { return isConstructed_; }
    const BuildingStats& getStats() const { return stats_; }

    // ✅ Position setter
    void setPosition(const glm::vec3& pos);

    // ✅ Building construction and health management
    void updateConstruction(float deltaTime);
    void takeDamage(float damage);
    void repair(float amount);

    // ✅ Get resource costs
    ResourceCost getBuildCost() const { return stats_.buildCost; }
    ResourceCost getRepairCost() const { return stats_.repairCost; }

    static ResourceCost getStaticCost(BuildingType type);

private:
    BuildingType type_;
    glm::vec3 position_;
    Mesh* mesh_;

    // ✅ New stats fields
    BuildingStats stats_;
    float currentHealth_;
    float buildProgress_;    // 0.0 to 1.0
    bool isConstructed_;

    glm::vec3 basePosition_;  // ✅ NEW: base of building (for clipping)
    float buildingHeight_;     // ✅ NEW: total height

    // ✅ Helper to initialize stats based on type
    void initializeStats();
};