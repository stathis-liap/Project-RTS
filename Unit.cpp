#include "Unit.h"
#include "Terrain.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>
#include <iostream>
#include <string>
#include "Pathfinder.h"
#include "Building.h"
#include <algorithm> // Required for std::shuffle
#include <random>    // Required for std::random_device

// Define static pointers so we load models only ONCE per game session
// CORRECT: These define the static members declared in Unit.h
SkinnedMesh* Unit::minionMesh = nullptr;
SkinnedMesh* Unit::warriorMesh = nullptr;
SkinnedMesh* Unit::mageMesh = nullptr;

// Constants
const float GATHER_RANGE = 6.0f;       // Distance to start chopping
const float GATHER_SPEED = 1.0f;       // Seconds per "chop"
const int   STAMINA_COST_PER_TREE = 1;
const int   RESOURCE_PER_TICK = 10;
const float STAMINA_DRAIN = 1.0f;

int Unit::NextID = 0;

// ---------------------------------------------------------------------
// CONSTRUCTOR
// ---------------------------------------------------------------------
Unit::Unit(UnitType type, const glm::vec3& pos, int teamID)
    : type_(type), position_(pos), teamID_(teamID), velocity_(0.0f), mesh_(nullptr)
{
    id_ = ++NextID; // ✅ Assign Unique ID

    // 1. Initialize Stats
    if (type_ == UnitType::WORKER) {
        maxHealth_ = 50;
        damage_ = 2; // Weak
        attackRange_ = 3.0f; // Melee
        attackCooldown_ = 1.0f;
    }
    else if (type_ == UnitType::MELEE) {
        maxHealth_ = 150;
        damage_ = 15;
        attackRange_ = 3.5f; // Melee reach
        attackCooldown_ = 1.2f;
    }
    else if (type_ == UnitType::RANGED) {
        maxHealth_ = 80;
        damage_ = 10;
        attackRange_ = 15.0f; // Shoots from far away
        attackCooldown_ = 1.5f;
    }
    currentHealth_ = maxHealth_;

    // 2. Load Models
    if (type_ == UnitType::WORKER) {
        if (!minionMesh) {
            std::cout << "Loading Worker Model..." << std::endl;
            minionMesh = new SkinnedMesh("models/Skeleton_Minion.fbx");
            minionMesh->SetupInstancing(); // ✅ IMPORTANT
        }
        mesh_ = minionMesh;
    }
    else if (type_ == UnitType::MELEE) {
        if (!warriorMesh) {
            std::cout << "Loading Warrior Model..." << std::endl;
            warriorMesh = new SkinnedMesh("models/Skeleton_Warrior.fbx");
            warriorMesh->SetupInstancing(); // ✅ IMPORTANT
        }
        mesh_ = warriorMesh;
    }
    else if (type_ == UnitType::RANGED) {
        if (!mageMesh) {
            std::cout << "Loading Mage Model..." << std::endl;
            mageMesh = new SkinnedMesh("models/Skeleton_Mage.fbx");
            mageMesh->SetupInstancing(); // ✅ IMPORTANT
        }
        mesh_ = mageMesh;
    }

    velocity_ = glm::vec3(0.0f);
    position_.y = 0.0f;
}

Unit::~Unit() {
    // We do NOT delete mesh_ here because it is shared/static.
}

// ---------------------------------------------------------------------
// TASK HELPERS
// ---------------------------------------------------------------------
void Unit::assignGatherTask(int obstacleID) {
    if (type_ != UnitType::WORKER) {
        std::cout << "Only workers can gather resources!" << std::endl;
        return;
    }
    taskQueue_.push_back(obstacleID);
}

void Unit::assignGatherQueue(const std::vector<int>& resourceIDs) {
    if (resourceIDs.empty()) return;

    // 1. Clear previous tasks
    taskQueue_.clear();
    m_HasTarget = false;
    state_ = UnitState::IDLE;

    // 2. Create a local copy to shuffle
    std::vector<int> shuffledResources = resourceIDs;

    // 3. Shuffle the list randomly
    // This ensures Unit A goes to Tree #1, while Unit B goes to Tree #5
    static std::random_device rd;
    static std::mt19937 g(rd());
    std::shuffle(shuffledResources.begin(), shuffledResources.end(), g);

    // 4. Fill the queue
    for (int id : shuffledResources) {
        taskQueue_.push_back(id);
    }

    // 5. Start immediately if we have a task
    if (!taskQueue_.empty()) {
        state_ = UnitState::IDLE; // The Update loop will pick up the task in the IDLE block
    }
}

void Unit::assignAttackQueue(const std::vector<Unit*>& enemies) {
    if (enemies.empty()) return;

    // 1. Create a local copy so we can shuffle it
    std::vector<Unit*> shuffledTargets = enemies;

    // 2. Shuffle the list randomly
    // This ensures 50 warriors don't all chase the exact same skeleton first
    static std::random_device rd;
    static std::mt19937 g(rd());
    std::shuffle(shuffledTargets.begin(), shuffledTargets.end(), g);

    // 3. Reset State
    taskQueue_.clear();
    m_HasTarget = false;
    m_Path.clear();
    attackQueue_.clear();

    // 4. Fill Queue with Shuffled IDs
    for (Unit* u : shuffledTargets) {
        if (u && u->getID() != id_) {
            attackQueue_.push_back(u->getID());
        }
    }

    // 5. Start attacking the first one immediately
    if (!attackQueue_.empty()) {
        targetID_ = attackQueue_.front();
        attackQueue_.pop_front();
        state_ = UnitState::ATTACKING;
    }
}

void Unit::assignAttackTask(Unit* enemy) {
    if (!enemy || enemy == this || enemy->getTeam() == teamID_) return;

    // ✅ FIX: Use ID instead of pointer
    targetID_ = enemy->getID();
    attackQueue_.clear(); // Clear any queued enemies if we manually clicked one

    state_ = UnitState::ATTACKING;

    // Clear other non-combat tasks
    taskQueue_.clear();
    m_HasTarget = false;
    m_Path.clear();
}

void Unit::assignAttackTask(Building* building) {
    if (!building || building->getTeam() == teamID_) return;

    targetBuilding_ = building;
    state_ = UnitState::ATTACKING_BUILDING;

    // Clear other targets
    targetID_ = -1;
    attackQueue_.clear();
    taskQueue_.clear();
    m_HasTarget = false;
    m_Path.clear();
}

void Unit::takeDamage(int dmg) {
    currentHealth_ -= dmg;
}

void Unit::update(float dt, const Terrain* terrain, const std::vector<std::unique_ptr<Unit>>& allUnits,
    Resources& globalResources, Environment* env, NavigationGrid* navGrid)
{
    // ==================================================================================
    // 1. STATE MACHINE (Logic)
    // ==================================================================================
    // 

    // --- STATE: IDLE (Looking for work) ---
    if (state_ == UnitState::IDLE && !taskQueue_.empty()) {
        currentTargetID_ = taskQueue_.front();
        if (env) {
            Obstacle* obs = env->getObstacleById(currentTargetID_);
            if (obs && obs->active) {
                // 1. Calculate direction to edge of tree
                glm::vec3 dir = obs->position - position_;
                if (glm::length(dir) > 0.001f) dir = glm::normalize(dir);
                else dir = glm::vec3(1, 0, 0);

                // 2. Target point just outside radius
                float stopDistance = obs->radius + 1.5f;
                glm::vec3 gatherSpot = obs->position - (dir * stopDistance);

                // 3. Find path
                std::vector<glm::vec3> path = Pathfinder::findPath(position_, gatherSpot, navGrid);

                if (!path.empty()) {
                    setPath(path);
                    state_ = UnitState::MOVING; // ✅ FIX: Move first, Gather later
                }
                else {
                    std::cout << "Path to resource blocked." << std::endl;
                    taskQueue_.pop_front();
                }
            }
            else {
                taskQueue_.pop_front();
            }
        }
    }

    // --- STATE: MOVING (Travel & Chase Logic) ---
    if (state_ == UnitState::MOVING) {

        // A. CHECK TRANSITIONS (Are we there yet?)
        // ----------------------------------------

        // 1. Moving to Attack UNIT
        if (targetID_ != -1) {
            Unit* targetUnit = nullptr;
            for (const auto& u : allUnits) { if (u->getID() == targetID_) { targetUnit = u.get(); break; } }

            if (!targetUnit || targetUnit->isDead()) {
                targetID_ = -1; // Target lost
                // Check queue or go idle (logic handled at end of block)
            }
            else {
                float dist = glm::distance(position_, targetUnit->getPosition());
                float reach = attackRange_ + 1.0f;

                if (dist <= reach) {
                    // ✅ ARRIVED -> Switch to Action
                    state_ = UnitState::ATTACKING;
                    velocity_ = glm::vec3(0.0f);
                    m_Path.clear();
                    m_HasTarget = false;
                }
                else {
                    // ✅ CHASE LOGIC (Moved here from Attacking state)
                    repathTimer_ += dt;
                    if (repathTimer_ > 0.5f || !m_HasTarget) {
                        repathTimer_ = 0.0f;
                        std::vector<glm::vec3> path = Pathfinder::findPath(position_, targetUnit->getPosition(), navGrid);
                        if (path.size() > 2) path.pop_back();
                        setPath(path);
                    }
                }
            }
        }
        // 2. Moving to Attack BUILDING
        else if (targetBuilding_) {
            if (targetBuilding_->isDead()) {
                targetBuilding_ = nullptr;
            }
            else {
                float buildingRadius = 14.0f;
                float effectiveRange = buildingRadius + attackRange_ + 5.0f;
                float dist = glm::distance(position_, targetBuilding_->getPosition());

                if (dist <= effectiveRange) {
                    // ✅ ARRIVED -> Switch to Action
                    state_ = UnitState::ATTACKING_BUILDING;
                    velocity_ = glm::vec3(0.0f);
                    m_Path.clear();
                    m_HasTarget = false;
                }
                // (Building is static, no need to re-path constantly)
            }
        }
        // 3. Moving to Gather RESOURCE
        else if (currentTargetID_ != -1 && env) {
            Obstacle* target = env->getObstacleById(currentTargetID_);
            if (target && target->active) {
                float dist = glm::distance(position_, target->position);
                float reach = target->radius + 5.0f;

                if (dist <= reach) {
                    // ✅ ARRIVED -> Switch to Action
                    state_ = UnitState::GATHERING;
                    velocity_ = glm::vec3(0.0f);
                    m_Path.clear();
                    m_HasTarget = false;
                }
            }
            else {
                currentTargetID_ = -1; // Resource gone
            }
        }

        // B. EXECUTE MOVEMENT (Standard Path Following)
        // ---------------------------------------------
        if (state_ == UnitState::MOVING) { // Only if we didn't switch state above
            if (m_HasTarget && !m_Path.empty()) {
                glm::vec3 targetPoint = m_Path.front();
                glm::vec3 dir = targetPoint - position_;
                dir.y = 0;
                float dist = glm::length(dir);
                float stopRadius = (m_Path.size() == 1) ? 1.0f : 0.5f;

                if (dist < stopRadius) {
                    m_Path.erase(m_Path.begin());
                    if (m_Path.empty()) {
                        m_HasTarget = false;
                        velocity_ = glm::vec3(0.0f);

                        // If path ended and we still haven't reached our "Action State",
                        // it means we are just moving to a spot (Right Click on Ground).
                        if (targetID_ == -1 && currentTargetID_ == -1 && !targetBuilding_) {
                            state_ = UnitState::IDLE;
                        }
                    }
                }
            }
            // Fail-safe
            if (!m_HasTarget && state_ == UnitState::MOVING) {
                // If we have a target but no path, try repath? Or Idle.
                // For now, Idle to prevent stuck state.
                if (targetID_ == -1 && currentTargetID_ == -1 && !targetBuilding_) {
                    state_ = UnitState::IDLE;
                }
            }
        }
    }

    // --- STATE: ATTACKING (Action Only) ---
    if (state_ == UnitState::ATTACKING) {
        Unit* targetUnit = nullptr;
        if (targetID_ != -1) {
            for (const auto& u : allUnits) { if (u->getID() == targetID_) { targetUnit = u.get(); break; } }
        }

        if (!targetUnit || targetUnit->isDead()) {
            // Target dead, check queue or Idle
            if (!attackQueue_.empty()) {
                targetID_ = attackQueue_.front();
                attackQueue_.pop_front();
                state_ = UnitState::MOVING; // ✅ Switch back to moving to chase new target
            }
            else {
                state_ = UnitState::IDLE;
                targetID_ = -1;
            }
        }
        else {
            // Check if target ran away
            float dist = glm::distance(position_, targetUnit->getPosition());
            float reach = attackRange_ + 1.0f;

            if (dist > reach) {
                state_ = UnitState::MOVING; // ✅ Switch back to moving to chase
            }
            else {
                // HIT LOGIC
                velocity_ = glm::vec3(0.0f); // Ensure we stay still
                attackTimer_ += dt;
                if (attackTimer_ >= attackCooldown_) {
                    attackTimer_ = 0.0f;
                    targetUnit->takeDamage(damage_);
                }
            }
        }
    }

    // --- STATE: ATTACKING BUILDING (Action Only) ---
    if (state_ == UnitState::ATTACKING_BUILDING) {
        if (!targetBuilding_ || targetBuilding_->isDead()) {
            state_ = UnitState::IDLE;
            targetBuilding_ = nullptr;
        }
        else {
            float buildingRadius = 14.0f;
            float effectiveRange = buildingRadius + attackRange_ + 5.0f;
            float dist = glm::distance(position_, targetBuilding_->getPosition());

            if (dist > effectiveRange) {
                state_ = UnitState::MOVING; // Go back to moving if pushed away
            }
            else {
                velocity_ = glm::vec3(0.0f);
                attackTimer_ += dt;
                if (attackTimer_ >= attackCooldown_) {
                    attackTimer_ = 0.0f;
                    targetBuilding_->takeDamage((float)damage_);
                }
            }
        }
    }

    // --- STATE: GATHERING (Action Only) ---
    if (state_ == UnitState::GATHERING && env) {
        Obstacle* target = env->getObstacleById(currentTargetID_);
        if (!target || !target->active) {
            state_ = UnitState::IDLE; // Resource gone
        }
        else {
            float dist = glm::distance(position_, target->position);
            float reach = target->radius + 5.0f;

            if (dist > reach) {
                state_ = UnitState::MOVING; // Go back to moving if pushed away
            }
            else {
                // CHOP LOGIC
                velocity_ = glm::vec3(0.0f);
                gatherTimer_ += dt;
                if (gatherTimer_ >= GATHER_SPEED) {
                    gatherTimer_ = 0.0f;
                    target->resourceAmount -= RESOURCE_PER_TICK;
                    if (target->type == ObstacleType::TREE) globalResources.addWood(RESOURCE_PER_TICK);
                    else globalResources.addRock(RESOURCE_PER_TICK);

                    if (target->resourceAmount <= 0) {
                        target->active = false;
                        if (navGrid) navGrid->updateArea(target->position, target->radius, false);
                        state_ = UnitState::IDLE;
                        if (!taskQueue_.empty()) taskQueue_.pop_front();
                    }
                }
            }
        }
    }

    // ==================================================================================
    // 2. PHYSICS & MOVEMENT (Steering Behaviors)
    // ==================================================================================
    // 

    glm::vec3 acc(0.0f);

    // Apply forces ONLY if we have a target or need to separate
    // ✅ Check state for movement:
    bool isMovingState = (state_ == UnitState::MOVING);

    if (isMovingState && m_HasTarget && !m_Path.empty()) {
        glm::vec3 target = m_Path.front();
        glm::vec3 dir = target - position_;
        dir.y = 0;
        dir = glm::normalize(dir);
        float dist = glm::distance(position_, target);

        // SEEK FORCE
        float moveSpeed = 50.0f;
        glm::vec3 seek = dir * moveSpeed;

        // Arrival Braking
        if (m_Path.size() == 1 && dist < 5.0f) {
            seek *= (dist / 5.0f);
        }
        acc += seek;
    }

    // SEPARATION FORCE (Apply in Idle AND when moving to avoid stacking)
    // We apply it slightly stronger in IDLE.
    if (!allUnits.empty()) {
        glm::vec3 sepForce(0.0f);
        int neighbors = 0;
        int checkCount = 8;
        size_t startIdx = (id_ * 37) % allUnits.size();

        for (int i = 0; i < checkCount; ++i) {
            size_t idx = (startIdx + i) % allUnits.size();
            const auto& other = allUnits[idx];
            if (other.get() == this) continue;

            float d = glm::distance(position_, other->getPosition());
            if (d < 3.0f && d > 0.01f) {
                glm::vec3 push = position_ - other->getPosition();
                push.y = 0;
                sepForce += glm::normalize(push) / d;
                neighbors++;
            }
        }
        if (neighbors > 0) {
            acc += sepForce * 60.0f;
        }
    }

    // INTEGRATION
    velocity_ += acc * dt;

    // Cap Max Speed
    float maxSpeed = 10.0f;
    if (glm::length(velocity_) > maxSpeed) {
        velocity_ = glm::normalize(velocity_) * maxSpeed;
    }

    // Friction
    if (!isMovingState) {
        velocity_ *= 0.5f; // High friction when Action/Idle
    }
    else {
        velocity_ *= 0.95f; // Low friction when Moving
    }

    if (glm::length(velocity_) < 0.1f) velocity_ = glm::vec3(0.0f);
    position_ += velocity_ * dt;

    // Terrain Clamp
    if (position_.x <= 0) position_.x = 0.1f;
    if (position_.x >= 512) position_.x = 511.9f;
    if (position_.z <= 0) position_.z = 0.1f;
    if (position_.z >= 512) position_.z = 511.9f;

    if (terrain) {
        position_.y = terrain->getHeightAt(position_.x, position_.z).y;
    }

    // =========================================================
    // FINAL PHYSICS CLAMP (The "Invisible Wall")
    // =========================================================
    // This stops units from being pushed into the border rocks by physics/separation.

    float mapSize = 512.0f;
    float borderSize = 30.0f; // Thickness of the rocks (Safety margin)

    if (position_.x < borderSize) position_.x = borderSize;
    if (position_.x > mapSize - borderSize) position_.x = mapSize - borderSize;

    if (position_.z < borderSize) position_.z = borderSize;
    if (position_.z > mapSize - borderSize) position_.z = mapSize - borderSize;

    // Now apply height
    if (terrain) {
        position_.y = terrain->getHeightAt(position_.x, position_.z).y;
    }
}

void Unit::setPath(const std::vector<glm::vec3>& newPath) {
    m_Path = newPath;
    m_HasTarget = (!m_Path.empty());

    // ✅ FIX: Automatically switch state to MOVING if we have a path
    if (m_HasTarget && state_ != UnitState::ATTACKING) {
        state_ = UnitState::MOVING;
    }
}

void Unit::draw(const glm::mat4& view, const glm::mat4& projection, GLuint shaderProgram, float currentTime) {
    if (!mesh_) return;

    // 1. SETUP MODEL MATRIX
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position_);

    // Rotate to face movement direction
    if (glm::length(velocity_) > 0.1f) {
        glm::vec3 direction = glm::normalize(velocity_);
        float angle = atan2(direction.x, direction.z);
        model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    float scaleVal = 3.0f;
    model = glm::scale(model, glm::vec3(scaleVal));

    // 2. SEND UNIFORMS
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "M"), 1, GL_FALSE, &model[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "V"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "P"), 1, GL_FALSE, &projection[0][0]);

    // 3. ANIMATION
    mesh_->UpdateAnimation(currentTime);
    const std::vector<glm::mat4>& transforms = mesh_->GetFinalBoneMatrices();

    for (unsigned int i = 0; i < transforms.size(); i++) {
        std::string name = "finalBonesMatrices[" + std::to_string(i) + "]";
        GLint loc = glGetUniformLocation(shaderProgram, name.c_str());
        if (loc != -1) {
            glUniformMatrix4fv(loc, 1, GL_FALSE, &transforms[i][0][0]);
        }
    }
    mesh_->Draw(shaderProgram);
}