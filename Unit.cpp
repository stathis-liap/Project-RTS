#include "Unit.h"
#include "Mesh.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>
#include <iostream>
#include <fstream>

static Mesh* sphereMesh = nullptr;

Unit::Unit(UnitType type, const glm::vec3& pos)
    : type_(type), position_(pos), velocity_(0.0f)
{
    if (!sphereMesh) {
        std::string path = "models/sphere.obj";
        std::cout << "Loading unit model: '" << path << "'" << std::endl;

        std::ifstream test(path);
        if (!test.is_open()) {
            std::cerr << "ERROR: Cannot find '" << path << "'! Make sure sphere.obj is in models/\n";
            // Fallback to a simple cube if sphere missing (optional)
            sphereMesh = new Mesh("models/cube.obj");
        }
        else {
            test.close();
            sphereMesh = new Mesh(path);
        }
    }

    mesh_ = sphereMesh;

    // Random initial direction and speed
    velocity_ = glm::ballRand(1.0f);  // random direction
    velocity_.y = 0.0f;               // keep on ground plane
    velocity_ = glm::normalize(velocity_) * (12.0f + glm::linearRand(0.0f, 8.0f));

    // Lift slightly above terrain
    position_.y = 1.0f;
}

Unit::~Unit()
{
    // Shared mesh, don't delete
}

void Unit::update(float dt)
{
    position_ += velocity_ * dt;

    // Random direction change (~every 2-4 seconds on average)
    if (glm::linearRand(0.0f, 1.0f) < dt * 0.3f) {  // 30% chance per second
        velocity_ = glm::ballRand(1.0f);
        velocity_.y = 0.0f;
        velocity_ = glm::normalize(velocity_) * (12.0f + glm::linearRand(0.0f, 8.0f));
    }

    // Bounce off map edges
    if (position_.x <= 0 || position_.x >= 512) velocity_.x *= -1.0f;
    if (position_.z <= 0 || position_.z >= 512) velocity_.z *= -1.0f;

    // Clamp to map
    position_.x = glm::clamp(position_.x, 0.0f, 512.0f);
    position_.z = glm::clamp(position_.z, 0.0f, 512.0f);
}

void Unit::draw(const glm::mat4& view, const glm::mat4& projection, GLuint shaderProgram)
{

    glm::mat4 model = glm::translate(glm::mat4(1.0f), position_);

    float scale = 1.0f;
    switch (type_) {
    case UnitType::WORKER:  scale = 1.0f; break;
    case UnitType::MELEE:   scale = 1.4f; break;
    case UnitType::RANGED:  scale = 1.2f; break;
    default:                scale = 1.0f; break;
    }
    model = glm::scale(model, glm::vec3(scale));

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "M"), 1, GL_FALSE, &model[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "V"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "P"), 1, GL_FALSE, &projection[0][0]);

    mesh_->draw();
}