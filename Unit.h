#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <deque>
#include "SkinnedMesh.h"
#include "Resource.h"
#include "Environment.h"

class NavigationGrid;
class Building; 

enum class UnitType { WORKER, MELEE, RANGED };
enum class UnitState { IDLE, MOVING, GATHERING, ATTACKING, ATTACKING_BUILDING };

class Unit {
public:
    Unit(UnitType type, const glm::vec3& pos, int teamID);
    ~Unit();

    void update(float dt, const Terrain* terrain, const std::vector<std::unique_ptr<Unit>>& allUnits,
        Resources& globalResources, Environment* env, NavigationGrid* navGrid);

    void draw(const glm::mat4& view, const glm::mat4& projection, GLuint shaderProgram, float currentTime);

    UnitType getType() const { return type_; }

    // Expose Meshes publicly so Main can use them
    static SkinnedMesh* minionMesh;
    static SkinnedMesh* warriorMesh;
    static SkinnedMesh* mageMesh;

    // Movement
    void setPath(const std::vector<glm::vec3>& newPath);
    void setSelected(bool s) { selected_ = s; }
    bool isSelected() const { return selected_; }
    glm::vec3 getPosition() const { return position_; }

    // Resource Logic
    void assignGatherTask(int obstacleID);
    bool hasStaminaForTask(int cost) const { return currentStamina_ >= cost; }
    float getStamina() const { return currentStamina_; }
    glm::vec3 getVelocity() const { return velocity_; }
    // Assigns a list of resources, but randomizes the order
    void assignGatherQueue(const std::vector<int>& resourceIDs);

    void clearTasks() {
        taskQueue_.clear();
        m_HasTarget = false;
        state_ = UnitState::IDLE;
        targetID_ = -1;
        attackQueue_.clear();
        targetBuilding_ = nullptr;
    }

    // Combat Logic
    void assignAttackTask(Unit* enemy);
    void assignAttackQueue(const std::vector<Unit*>& enemies);
    float repathTimer_ = 0.0f;

    //Declare the Building Attack Function
    void assignAttackTask(Building* building);

    void takeDamage(int dmg);
    bool isDead() const { return currentHealth_ <= 0; }
    int getTeam() const { return teamID_; }
    bool isAttacking() const { return state_ == UnitState::ATTACKING; }
    bool isAttackingBuilding() const { return state_ == UnitState::ATTACKING_BUILDING; }
    bool isGathering() const { return state_ == UnitState::GATHERING; }
    void explode() { currentHealth_ = -1.0f; } // Instantly kill unit

    int getID() const { return id_; }
    UnitState getState() const { return state_; }

private:
    static int NextID;
    int id_;

    // Combat Variables
    int targetID_ = -1;
    std::deque<int> attackQueue_;

    // Declare the Building Target Pointer
    Building* targetBuilding_ = nullptr;

    // Standard Variables
    UnitType type_;
    int teamID_;

    glm::vec3 position_;
    glm::vec3 velocity_;
    SkinnedMesh* mesh_;
    bool selected_ = false;

    // Movement Path
    std::vector<glm::vec3> m_Path;
    bool m_HasTarget = false;

    // State
    UnitState state_ = UnitState::IDLE;

    // Worker Stats
    float currentStamina_ = 100.0f;
    std::deque<int> taskQueue_;
    int currentTargetID_ = -1;
    float gatherTimer_ = 0.0f;

    // Combat Stats
    int maxHealth_;
    int currentHealth_;
    int damage_;
    float attackRange_;
    float attackCooldown_;
    float attackTimer_ = 0.0f;
};