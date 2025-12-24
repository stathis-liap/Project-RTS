#ifndef UNIT_H
#define UNIT_H

#include <glm/glm.hpp>
#include <GL/glew.h>
#include <string>

class Mesh;

enum class UnitType {
    WORKER,
    MELEE,
    RANGED,
    NONE
};

class Unit {
public:
    Unit(UnitType type, const glm::vec3& position);
    ~Unit();

    void update(float dt);  // Random movement
    void draw(const glm::mat4& view, const glm::mat4& projection, GLuint shaderProgram);

    UnitType getType() const { return type_; }
    glm::vec3 getPosition() const { return position_; }
    Mesh* getMesh() const { return mesh_; }

private:
    UnitType type_;
    glm::vec3 position_;
    glm::vec3 velocity_;
    Mesh* mesh_ = nullptr;
};

#endif