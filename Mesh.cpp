#include "Mesh.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <tuple> // Use tuple for (v, vt, vn) key
#include <stdexcept>
#include <cstring> // for strcmp

Mesh::Mesh(const std::string& filepath)
{
    // Raw data from file
    std::vector<glm::vec3> temp_pos;
    std::vector<glm::vec2> temp_uv;
    std::vector<glm::vec3> temp_norm;

    // Final buffers for OpenGL
    std::vector<glm::vec3> final_pos;
    std::vector<glm::vec2> final_uv;
    std::vector<glm::vec3> final_norm;
    std::vector<unsigned int> indices;

    // Map to deduplicate vertices: (v_idx, vt_idx, vn_idx) -> final_index
    // Using a tuple allows us to uniquely identify a vertex by all 3 attributes
    std::map<std::tuple<int, int, int>, unsigned int> vertexMap;

    std::ifstream file(filepath);
    if (!file.is_open())
        throw std::runtime_error("Cannot open OBJ: " + filepath);

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        // --------------------------------------------------------------
        //  Vertex Position (v)
        // --------------------------------------------------------------
        if (prefix == "v") {
            glm::vec3 p; iss >> p.x >> p.y >> p.z;
            temp_pos.push_back(p);
        }
        // --------------------------------------------------------------
        //  Texture Coordinate (vt)
        // --------------------------------------------------------------
        else if (prefix == "vt") {
            glm::vec2 uv; iss >> uv.x >> uv.y;
            temp_uv.push_back(uv);
        }
        // --------------------------------------------------------------
        //  Vertex Normal (vn)
        // --------------------------------------------------------------
        else if (prefix == "vn") {
            glm::vec3 n; iss >> n.x >> n.y >> n.z;
            temp_norm.push_back(n);
        }
        // --------------------------------------------------------------
        //  Face (f) - Handles Triangles & Quads, various formats
        // --------------------------------------------------------------
        else if (prefix == "f")
        {
            // We read the rest of the line to handle flexible numbers of vertices (3 or 4)
            std::string remainder;
            std::getline(iss, remainder);
            std::stringstream ss(remainder);
            std::string segment;

            std::vector<std::tuple<int, int, int>> faceVertices;

            while (ss >> segment) {
                int v = 0, vt = 0, vn = 0;

                // Check format by counting slashes
                int slashCount = 0;
                for (char c : segment) if (c == '/') slashCount++;

                // Format: v//vn (common in non-textured meshes)
                if (slashCount == 2 && segment.find("//") != std::string::npos) {
                    sscanf(segment.c_str(), "%d//%d", &v, &vn);
                }
                // Format: v/vt/vn (standard full format)
                else if (slashCount == 2) {
                    sscanf(segment.c_str(), "%d/%d/%d", &v, &vt, &vn);
                }
                // Format: v/vt (no normals)
                else if (slashCount == 1) {
                    sscanf(segment.c_str(), "%d/%d", &v, &vt);
                }
                // Format: v (position only)
                else {
                    sscanf(segment.c_str(), "%d", &v);
                }

                faceVertices.push_back(std::make_tuple(v, vt, vn));
            }

            // Now triangulate the face (Ear Clipping / Fan triangulation)
            // A face with N vertices creates (N-2) triangles: 0-1-2, 0-2-3, etc.
            if (faceVertices.size() < 3) continue; // Degenerate line/point

            for (size_t i = 1; i + 1 < faceVertices.size(); ++i) {
                // Triangle vertices: 0, i, i+1
                std::tuple<int, int, int> tri[3] = {
                    faceVertices[0],
                    faceVertices[i],
                    faceVertices[i + 1]
                };

                for (int k = 0; k < 3; ++k) {
                    int v_idx = std::get<0>(tri[k]);
                    int vt_idx = std::get<1>(tri[k]);
                    int vn_idx = std::get<2>(tri[k]);

                    // Handle negative indices (OBJ relative indexing)
                    if (v_idx < 0) v_idx = temp_pos.size() + v_idx + 1;
                    if (vt_idx < 0) vt_idx = temp_uv.size() + vt_idx + 1;
                    if (vn_idx < 0) vn_idx = temp_norm.size() + vn_idx + 1;

                    // Create key for deduplication
                    auto key = std::make_tuple(v_idx, vt_idx, vn_idx);

                    if (vertexMap.find(key) == vertexMap.end()) {
                        // New unique vertex combination
                        unsigned int newIndex = (unsigned int)final_pos.size();
                        vertexMap[key] = newIndex;
                        indices.push_back(newIndex);

                        // Push Position (Required)
                        final_pos.push_back(temp_pos[v_idx - 1]);

                        // Push UV (Optional, default 0,0)
                        if (vt_idx > 0 && vt_idx <= (int)temp_uv.size())
                            final_uv.push_back(temp_uv[vt_idx - 1]);
                        else
                            final_uv.push_back(glm::vec2(0.0f));

                        // Push Normal (Optional, default 0,1,0)
                        if (vn_idx > 0 && vn_idx <= (int)temp_norm.size())
                            final_norm.push_back(temp_norm[vn_idx - 1]);
                        else
                            final_norm.push_back(glm::vec3(0, 1, 0));
                    }
                    else {
                        // Reuse existing index
                        indices.push_back(vertexMap[key]);
                    }
                }
            }
        }
    }

    indexCount = static_cast<GLsizei>(indices.size());

    // --------------------------------------------------------------
    //  Build interleaved VBO (pos + uv + norm)
    // --------------------------------------------------------------
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    // Structure matching our interleaved data
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 norm; // Swapped order to match your original shader binding if needed, 
        // BUT standard is often Pos, Normal, UV. 
        // Based on your previous code it was Pos, Norm. 
        // I will add UV support here because new models have it.
        glm::vec2 uv;
    };

    std::vector<Vertex> data;
    data.reserve(final_pos.size());
    for (size_t i = 0; i < final_pos.size(); ++i) {
        data.push_back({ final_pos[i], final_norm[i], final_uv[i] });
    }

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(Vertex), data.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Attribute 0: Position (3 floats)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));

    // Attribute 1: Normal (3 floats)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, norm));

    // Attribute 2: UV (2 floats) - NEW! (Optional for your current shader, but good for future)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));

    glBindVertexArray(0);
}

Mesh::~Mesh()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

void Mesh::draw() const
{
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}