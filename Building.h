#ifndef BUILDING_H
#define BUILDING_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GL/glew.h>
#include <string>
#include <memory>  // ← ADD THIS for std::unique_ptr / shared_ptr if needed later

class Mesh;

enum class BuildingType {
    TOWN_CENTER,
    BARRACKS
};

// Forward declare UnitType to avoid circular includes
enum class UnitType : int;  // forward declaration only

class Building {
public:
    Building(BuildingType type, const glm::vec3& position, const std::string& meshPath = "");
    ~Building();

    void draw(const glm::mat4& view, const glm::mat4& projection, GLuint shaderProgram);

    // For shadow pass: draw with custom model matrix and shader
    void drawShadow(const glm::mat4& modelMatrix, GLuint shadowShader);

    BuildingType getType() const { return type_; }
    glm::vec3 getPosition() const { return position_; }
    //UnitType getSpawnableUnit() const;
    Mesh* getMesh() const { return mesh_; }

private:
    BuildingType type_;
    glm::vec3 position_;
    Mesh* mesh_ = nullptr;
};

#endif // BUILDING_H