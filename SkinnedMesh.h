#pragma once
#include <vector>
#include <string>
#include <map>
#include <iostream>
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

    // Instancing Functions
    void SetupInstancing();
    void DrawInstanced(GLuint shaderProgram, const std::vector<glm::mat4>& models);

    // ✅ NEW: Animation System
    // In SkinnedMesh.h
    void LoadAnimation(const std::string& filePath, const std::string& animationName, int index = 0);
    void PlayAnimation(const std::string& animationName);

    void UpdateAnimation(float timeInSeconds);

    // Debugging
    void PrintHierarchy(const aiNode* pNode, int depth);

    // Getters
    auto& GetBoneInfoMap() { return m_BoneInfoMap; }
    int& GetBoneCount() { return m_BoneCounter; }
    std::vector<glm::mat4> GetFinalBoneMatrices() { return m_FinalBoneMatrices; }

private:
    GLuint VAO = 0, VBO = 0, EBO = 0;
    GLuint instanceVBO = 0;

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::map<std::string, BoneInfo> m_BoneInfoMap;
    int m_BoneCounter = 0;

    // --- ASSIMP RESOURCES ---
    const aiScene* m_Scene = nullptr;   // The Mesh Scene
    Assimp::Importer m_Importer;        // The Mesh Importer

    // ✅ NEW: Animation Storage
    // We need to keep the importers alive, otherwise the pointers in m_Animations become invalid.
    std::vector<Assimp::Importer*> m_AnimationImporters;
    std::map<std::string, aiAnimation*> m_Animations;
    aiAnimation* m_CurrentAnimation = nullptr;

    std::vector<glm::mat4> m_FinalBoneMatrices;
    glm::mat4 m_GlobalInverseTransform;

    // Internal Helpers
    void setupMesh();
    void SetVertexBoneData(Vertex& vertex, int boneID, float weight);
    glm::mat4 ConvertMatrix(const aiMatrix4x4& from);
    void ExtractBoneWeightForVertices(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene, unsigned int baseVertex);

    void ReadNodeHierarchy(float animationTime, const aiNode* pNode, const glm::mat4& parentTransform);
    const aiNodeAnim* FindNodeAnim(const aiAnimation* animation, const std::string& nodeName);

    glm::vec3 InterpolatePosition(float animationTime, const aiNodeAnim* nodeAnim);
    glm::quat InterpolateRotation(float animationTime, const aiNodeAnim* nodeAnim);
    glm::vec3 InterpolateScale(float animationTime, const aiNodeAnim* nodeAnim);
};