// Mesh.cpp
#include "Mesh.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <utility>      // std::pair
#include <stdexcept>

Mesh::Mesh(const std::string& filepath)
{
    std::vector<glm::vec3> temp_pos;
    std::vector<glm::vec2> temp_uv;   // not used now, but kept for future
    std::vector<glm::vec3> temp_norm;

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<unsigned int> indices;

    std::map<std::pair<int, int>, unsigned int> vertexMap; // (v, vn) -> final index

    std::ifstream file(filepath);
    if (!file.is_open())
        throw std::runtime_error("Cannot open OBJ: " + filepath);

    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        // --------------------------------------------------------------
        //  Vertex data
        // --------------------------------------------------------------
        if (prefix == "v") {
            glm::vec3 p; iss >> p.x >> p.y >> p.z;
            temp_pos.push_back(p);
        }
        else if (prefix == "vt") {
            glm::vec2 uv; iss >> uv.x >> uv.y;
            temp_uv.push_back(uv);
        }
        else if (prefix == "vn") {
            glm::vec3 n; iss >> n.x >> n.y >> n.z;
            temp_norm.push_back(n);
        }

        // --------------------------------------------------------------
        //  Face (supports v//vn  and  v/vt/vn)
        // --------------------------------------------------------------
        else if (prefix == "f")
        {
            std::string token[3];
            iss >> token[0] >> token[1] >> token[2];

            // each token:  v   or   v/vt   or   v//vn   or   v/vt/vn
            int v[3] = { 0,0,0 }, vt[3] = { 0,0,0 }, vn[3] = { 0,0,0 };

            for (int i = 0; i < 3; ++i)
            {
                std::istringstream tss(token[i]);
                std::string part;
                int idx = 0;
                while (std::getline(tss, part, '/'))
                {
                    if (part.empty()) { ++idx; continue; }
                    int val = std::stoi(part);
                    if (idx == 0) v[i] = val;
                    else if (idx == 1) vt[i] = val;
                    else if (idx == 2) vn[i] = val;
                    ++idx;
                }
            }

            // ----- safety checks -----
            for (int i = 0; i < 3; ++i)
            {
                if (v[i]  < 1 || v[i]  > static_cast<int>(temp_pos.size()) ||
                    vn[i] < 1 || vn[i] > static_cast<int>(temp_norm.size()))
                {
                    std::cerr << "OBJ index out of range in " << filepath
                        << "  v=" << v[i] << " (max " << temp_pos.size()
                        << ")  vn=" << vn[i] << " (max " << temp_norm.size() << ")\n";
                    continue;   // skip this triangle
                }
            }

            // ----- deduplicate (v, vn) -----
            for (int i = 0; i < 3; ++i)
            {
                std::pair<int, int> key{ v[i], vn[i] };
                auto it = vertexMap.find(key);
                if (it == vertexMap.end())
                {
                    unsigned int idx = static_cast<unsigned int>(positions.size());
                    positions.push_back(temp_pos[v[i] - 1]);
                    normals.push_back(temp_norm[vn[i] - 1]);
                    vertexMap[key] = idx;
                    indices.push_back(idx);
                }
                else
                    indices.push_back(it->second);
            }
        }
    }

    indexCount = static_cast<GLsizei>(indices.size());

    // --------------------------------------------------------------
    //  Build interleaved VBO (pos + normal)
    // --------------------------------------------------------------
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);

    struct Vertex { glm::vec3 pos; glm::vec3 norm; };
    std::vector<Vertex> data;
    data.reserve(positions.size());
    for (size_t i = 0; i < positions.size(); ++i)
        data.push_back({ positions[i], normals[i] });

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
        data.size() * sizeof(Vertex),
        data.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        indices.size() * sizeof(unsigned int),
        indices.data(),
        GL_STATIC_DRAW);

    // position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        (void*)offsetof(Vertex, pos));

    // normal attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        (void*)offsetof(Vertex, norm));

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