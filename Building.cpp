#include "Building.h"
#include "Mesh.h"
#include <iostream>
#include <fstream>
#include <stdexcept>

Building::Building(BuildingType type, const glm::vec3& pos, const std::string& meshPath)
    : type_(type), position_(pos), mesh_(nullptr)
{
    std::string path = meshPath;
    if (path.empty()) {
        path = "models/cube.obj";  // fallback for all types
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
            throw; // if even cube fails, crash
        }
    }

    // Adjust Y position so building sits ON the terrain, not inside it
    float scale = (type_ == BuildingType::TOWN_CENTER) ? 11.0f : 8.0f;
    position_.y += scale * 0.5f;  // assuming mesh is centered at origin (-0.5 to +0.5)
}

Building::~Building()
{
    delete mesh_;
    mesh_ = nullptr;
}

void Building::draw(const glm::mat4& view, const glm::mat4& projection, GLuint shaderProgram)
{
   

    glm::mat4 model = glm::translate(glm::mat4(1.0f), position_);

    float scale = (type_ == BuildingType::TOWN_CENTER) ? 11.0f : 8.0f;
    model = glm::scale(model, glm::vec3(scale));

    GLuint locM = glGetUniformLocation(shaderProgram, "M");
    GLuint locV = glGetUniformLocation(shaderProgram, "V");
    GLuint locP = glGetUniformLocation(shaderProgram, "P");

    if (locM != -1) glUniformMatrix4fv(locM, 1, GL_FALSE, &model[0][0]);
    if (locV != -1) glUniformMatrix4fv(locV, 1, GL_FALSE, &view[0][0]);
    if (locP != -1) glUniformMatrix4fv(locP, 1, GL_FALSE, &projection[0][0]);

    if (mesh_) mesh_->draw();
}



//UnitType Building::getSpawnableUnit() const
//{
//    switch (type_) {
//    case BuildingType::TOWN_CENTER: return UnitType::WORKER;
//    case BuildingType::BARRACKS:     return UnitType::MELEE;
//    default:                        return UnitType::WORKER;
//    }
//}