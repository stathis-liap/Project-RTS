#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <deque>
#include "SkinnedMesh.h"
#include "Resource.h"
#include "Environment.h"

class NavigationGrid;

enum class UnitType { WORKER, MELEE, RANGED };
enum class UnitState { IDLE, MOVING, GATHERING, ATTACKING };

class Unit {
public:
    Unit(UnitType type, const glm::vec3& pos, int teamID);
    ~Unit();

    void update(float dt, const Terrain* terrain, const std::vector<std::unique_ptr<Unit>>& allUnits,
        Resources& globalResources, Environment* env, NavigationGrid* navGrid);

    void draw(const glm::mat4& view, const glm::mat4& projection, GLuint shaderProgram, float currentTime);

    // Movement
    void setPath(const std::vector<glm::vec3>& newPath);
    void setSelected(bool s) { selected_ = s; }
    bool isSelected() const { return selected_; }
    glm::vec3 getPosition() const { return position_; }

    // Resource Logic
    void assignGatherTask(int obstacleID);
    bool hasStaminaForTask(int cost) const { return currentStamina_ >= cost; }
    float getStamina() const { return currentStamina_; }

    // ✅ FIX: Update clearTasks to use targetID_
    void clearTasks() {
        taskQueue_.clear();
        m_HasTarget = false;
        state_ = UnitState::IDLE;
        targetID_ = -1;       // <--- FIXED (was targetUnit_ = nullptr)
        attackQueue_.clear(); // <--- FIXED (Clear the attack list too)
    }

    // Combat Logic
    void assignAttackTask(Unit* enemy);
    void assignAttackQueue(const std::vector<Unit*>& enemies); // New Queue function
    void takeDamage(int dmg);
    bool isDead() const { return currentHealth_ <= 0; }
    int getTeam() const { return teamID_; }
    bool isAttacking() const { return state_ == UnitState::ATTACKING; }
    bool isGathering() const { return state_ == UnitState::GATHERING; }

    // ✅ ID System
    int getID() const { return id_; }

private:
    static int NextID;
    int id_;

    // ✅ ID-Based Targeting
    int targetID_ = -1;
    std::deque<int> attackQueue_;

    UnitType type_;
    int teamID_;

    glm::vec3 position_;
    glm::vec3 velocity_;
    SkinnedMesh* mesh_;
    bool selected_ = false;

    std::vector<glm::vec3> m_Path;
    bool m_HasTarget = false;
    UnitState state_ = UnitState::IDLE;

    float currentStamina_ = 100.0f;
    std::deque<int> taskQueue_;
    int currentTargetID_ = -1;
    float gatherTimer_ = 0.0f;

    int maxHealth_;
    int currentHealth_;
    int damage_;
    float attackRange_;
    float attackCooldown_;
    float attackTimer_ = 0.0f;
};