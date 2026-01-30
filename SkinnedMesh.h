#pragma once
#include <vector>
#include <string>
#include <map>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#define MAX_BONE_INFLUENCE 4

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    int m_BoneIDs[MAX_BONE_INFLUENCE] = { -1, -1, -1, -1 };
    float m_Weights[MAX_BONE_INFLUENCE] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

struct BoneInfo {
    int id = -1;
    glm::mat4 offset = glm::mat4(1.0f);
};

class SkinnedMesh {
public:
    SkinnedMesh(const std::string& path);
    ~SkinnedMesh();

    void Draw(GLuint shaderProgram);
    void UpdateAnimation(float timeInSeconds); // ✅ NEW
    // Inside SkinnedMesh.h, under private or public:
    void PrintHierarchy(const aiNode* pNode, int depth);

    auto& GetBoneInfoMap() { return m_BoneInfoMap; }
    int& GetBoneCount() { return m_BoneCounter; }
    std::vector<glm::mat4> GetFinalBoneMatrices() { return m_FinalBoneMatrices; } // ✅ NEW

private:
    GLuint VAO = 0, VBO = 0, EBO = 0;

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::map<std::string, BoneInfo> m_BoneInfoMap;
    int m_BoneCounter = 0;

    // ✅ NEW: Animation data
    const aiScene* m_Scene = nullptr;
    Assimp::Importer m_Importer;
    std::vector<glm::mat4> m_FinalBoneMatrices;
    glm::mat4 m_GlobalInverseTransform;

    void setupMesh();
    void SetVertexBoneData(Vertex& vertex, int boneID, float weight);
    glm::mat4 ConvertMatrix(const aiMatrix4x4& from);
    void ExtractBoneWeightForVertices(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene, unsigned int baseVertex);

    // ✅ NEW: Animation functions
    void ReadNodeHierarchy(float animationTime, const aiNode* pNode, const glm::mat4& parentTransform);
    const aiNodeAnim* FindNodeAnim(const aiAnimation* animation, const std::string& nodeName);
    glm::vec3 InterpolatePosition(float animationTime, const aiNodeAnim* nodeAnim);
    glm::quat InterpolateRotation(float animationTime, const aiNodeAnim* nodeAnim);
    glm::vec3 InterpolateScale(float animationTime, const aiNodeAnim* nodeAnim);
};