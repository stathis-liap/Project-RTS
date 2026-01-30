#include "Unit.h"
#include "Terrain.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>
#include <iostream>
#include <string>
#include "Pathfinder.h"

// Define static pointers so we load models only ONCE per game session
static SkinnedMesh* minionMesh = nullptr;
static SkinnedMesh* warriorMesh = nullptr;
static SkinnedMesh* mageMesh = nullptr;

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
        }
        mesh_ = minionMesh;
    }
    else if (type_ == UnitType::MELEE) {
        if (!warriorMesh) {
            std::cout << "Loading Warrior Model..." << std::endl;
            warriorMesh = new SkinnedMesh("models/Skeleton_Warrior.fbx");
        }
        mesh_ = warriorMesh;
    }
    else if (type_ == UnitType::RANGED) {
        if (!mageMesh) {
            std::cout << "Loading Mage Model..." << std::endl;
            mageMesh = new SkinnedMesh("models/Skeleton_Mage.fbx");
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

void Unit::assignAttackQueue(const std::vector<Unit*>& enemies) {
    if (enemies.empty()) return;

    // Clear old state
    taskQueue_.clear();
    m_HasTarget = false;
    m_Path.clear();
    attackQueue_.clear();

    // Fill Queue with IDs
    for (Unit* u : enemies) {
        if (u && u->getID() != id_) {
            attackQueue_.push_back(u->getID());
        }
    }

    // Start attacking the first one immediately
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

void Unit::takeDamage(int dmg) {
    currentHealth_ -= dmg;
}

// ---------------------------------------------------------------------
// UPDATE LOOP
// ---------------------------------------------------------------------
void Unit::update(float dt, const Terrain* terrain, const std::vector<std::unique_ptr<Unit>>& allUnits,
    Resources& globalResources, Environment* env, NavigationGrid* navGrid)
{
    // ==================================================================================
    // 1. STATE MACHINE (Brain)
    // ==================================================================================

    // --- STATE: IDLE (Looking for work) ---
    if (state_ == UnitState::IDLE && !taskQueue_.empty()) {
        currentTargetID_ = taskQueue_.front();

        if (env) {
            Obstacle* obs = env->getObstacleById(currentTargetID_);
            if (obs && obs->active) {
                if (currentStamina_ < STAMINA_COST_PER_TREE) {
                    std::cout << "Worker is too tired!" << std::endl;
                    taskQueue_.clear();
                }
                else {
                    std::vector<glm::vec3> path = Pathfinder::findPath(position_, obs->position, navGrid);
                    if (path.size() > 1) path.pop_back();
                    setPath(path);
                    state_ = UnitState::MOVING;
                }
            }
            else {
                taskQueue_.pop_front();
            }
        }
    }

    // --- STATE: ATTACKING (Combat) ---
    if (state_ == UnitState::ATTACKING) {

        // 1. Resolve Target Pointer from ID
        Unit* targetUnit = nullptr;
        if (targetID_ != -1) {
            // Linear search is fine for small counts (Use hash map optimization later if >1000 units)
            for (const auto& u : allUnits) {
                if (u->getID() == targetID_) {
                    targetUnit = u.get();
                    break;
                }
            }
        }

        // 2. If Target is Invalid or Dead -> Next Target
        if (!targetUnit || targetUnit->isDead()) {
            if (!attackQueue_.empty()) {
                // ✅ Auto-switch to next enemy
                targetID_ = attackQueue_.front();
                attackQueue_.pop_front();
                // Loop continues next frame to find this new target
            }
            else {
                // Queue empty, job done
                state_ = UnitState::IDLE;
                targetID_ = -1;
            }
        }
        else {
            // 3. Fight Logic (Same as before)
            float dist = glm::distance(position_, targetUnit->getPosition());

            if (dist <= attackRange_) {
                // In Range: Hit
                velocity_ = glm::vec3(0.0f);
                m_Path.clear();
                m_HasTarget = false;

                attackTimer_ += dt;
                if (attackTimer_ >= attackCooldown_) {
                    attackTimer_ = 0.0f;
                    targetUnit->takeDamage(damage_);
                    std::cout << "Unit " << id_ << " hit Target " << targetID_ << std::endl;
                }
            }
            else {
                // Out of Range: Chase
                if (!m_HasTarget) {
                    std::vector<glm::vec3> path = Pathfinder::findPath(position_, targetUnit->getPosition(), navGrid);
                    if (path.size() > 1) path.pop_back();
                    setPath(path);
                }
            }
        }
    }

    // --- STATE: MOVING (Walking to target) ---
    if (state_ == UnitState::MOVING) {
        if (env && currentTargetID_ != -1 && !taskQueue_.empty()) {
            Obstacle* target = env->getObstacleById(currentTargetID_);
            if (target) {
                float dist = glm::distance(position_, target->position);
                bool closeEnough = (dist < GATHER_RANGE);
                bool pathFinished = (!m_HasTarget || m_Path.empty());

                if (closeEnough || (pathFinished && dist < GATHER_RANGE + 3.0f)) {
                    velocity_ = glm::vec3(0.0f);
                    m_Path.clear();
                    m_HasTarget = false;
                    state_ = UnitState::GATHERING;
                    std::cout << "Arrived at resource." << std::endl;
                }
            }
        }

        if (!m_HasTarget && state_ == UnitState::MOVING) {
            state_ = UnitState::IDLE;
        }
    }

    // --- STATE: GATHERING (Chopping/Mining) ---
    else if (state_ == UnitState::GATHERING) {
        Obstacle* target = (env) ? env->getObstacleById(currentTargetID_) : nullptr;

        if (!target || !target->active) {
            state_ = UnitState::IDLE;
            if (!taskQueue_.empty()) taskQueue_.pop_front();
        }
        else {
            gatherTimer_ += dt;
            if (gatherTimer_ >= GATHER_SPEED) {
                gatherTimer_ = 0.0f;

                target->resourceAmount -= RESOURCE_PER_TICK;
                currentStamina_ -= STAMINA_DRAIN;

                if (target->type == ObstacleType::TREE) globalResources.addWood(RESOURCE_PER_TICK);
                else globalResources.addRock(RESOURCE_PER_TICK);

                if (target->resourceAmount <= 0) {
                    target->active = false;
                    if (navGrid) navGrid->updateArea(target->position, target->radius, false);
                    state_ = UnitState::IDLE;
                    if (!taskQueue_.empty()) taskQueue_.pop_front();
                }

                if (currentStamina_ <= 0) {
                    state_ = UnitState::IDLE;
                    taskQueue_.clear();
                }
            }
        }
    }

    // ==================================================================================
    // 2. PHYSICS & MOVEMENT (Steering + Separation)
    // ==================================================================================

    // Only move if we are NOT gathering
    // Allow movement if Attacking but chasing (has target)
    bool shouldMove = (state_ == UnitState::MOVING) ||
        (state_ == UnitState::ATTACKING && m_HasTarget) ||
        (state_ == UnitState::IDLE && m_HasTarget == false);

    if (shouldMove && state_ != UnitState::GATHERING)
    {
        glm::vec3 moveForce(0.0f);

        // A. Path Following
        if (m_HasTarget && !m_Path.empty()) {
            glm::vec3 targetPoint = m_Path.front();
            glm::vec3 dir = targetPoint - position_;
            dir.y = 0;

            float dist = glm::length(dir);
            if (dist < 0.5f) {
                m_Path.erase(m_Path.begin());
                if (m_Path.empty()) {
                    m_HasTarget = false;
                    velocity_ = glm::vec3(0.0f);
                }
            }
            else {
                moveForce = glm::normalize(dir) * 10.0f;
            }
        }

        // B. Separation (Don't Clip)
        glm::vec3 separationForce(0.0f);
        float separationRadius = 2.5f;
        int neighbors = 0;

        for (const auto& other : allUnits) {
            if (other.get() == this) continue;
            float d = glm::distance(position_, other->getPosition());
            if (d < separationRadius && d > 0.001f) {
                glm::vec3 pushDir = position_ - other->getPosition();
                pushDir.y = 0;
                pushDir = glm::normalize(pushDir);
                separationForce += pushDir / d;
                neighbors++;
            }
        }

        if (neighbors > 0) {
            separationForce /= (float)neighbors;
            separationForce *= 15.0f;
        }

        // C. Apply Forces
        if (m_HasTarget || glm::length(separationForce) > 0.1f) {
            glm::vec3 finalVelocity = moveForce + separationForce;
            if (glm::length(finalVelocity) > 10.0f) {
                finalVelocity = glm::normalize(finalVelocity) * 10.0f;
            }
            velocity_ = finalVelocity;
            position_ += velocity_ * dt;
        }
        else {
            velocity_ = glm::vec3(0.0f);
        }
    }

    // ==================================================================================
    // 3. TERRAIN CLAMPING
    // ==================================================================================
    if (position_.x <= 0) position_.x = 0.1f;
    if (position_.x >= 512) position_.x = 511.9f;
    if (position_.z <= 0) position_.z = 0.1f;
    if (position_.z >= 512) position_.z = 511.9f;

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