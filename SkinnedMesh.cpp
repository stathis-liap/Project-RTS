#include "SkinnedMesh.h"
#include <iostream>
#include <vector>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Helper to convert Assimp matrix to GLM
glm::mat4 SkinnedMesh::ConvertMatrix(const aiMatrix4x4& from) {
    glm::mat4 to;
    to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
    to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
    to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
    to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
    return to;
}

// Debug function to print scene tree
void SkinnedMesh::PrintHierarchy(const aiNode* pNode, int depth) {
    std::string indent(depth * 4, ' ');
    std::string nodeName(pNode->mName.data);

    std::cout << indent << "Node: " << nodeName;

    if (m_BoneInfoMap.find(nodeName) != m_BoneInfoMap.end()) {
        std::cout << " [BONE ID: " << m_BoneInfoMap[nodeName].id << "]";
    }
    if (pNode->mNumMeshes > 0) {
        std::cout << "  <-- HAS MESH (" << pNode->mNumMeshes << ")";
    }
    std::cout << std::endl;

    for (unsigned int i = 0; i < pNode->mNumChildren; i++) {
        PrintHierarchy(pNode->mChildren[i], depth + 1);
    }
}

SkinnedMesh::SkinnedMesh(const std::string& path) {
    m_Scene = m_Importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);

    if (!m_Scene || m_Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !m_Scene->mRootNode) {
        std::cerr << "ERROR::ASSIMP:: " << m_Importer.GetErrorString() << std::endl;
        return;
    }

    m_GlobalInverseTransform = glm::mat4(1.0f);

    std::cout << "\n========== SCENE HIERARCHY ==========" << std::endl;
    PrintHierarchy(m_Scene->mRootNode, 0);
    std::cout << "=====================================\n" << std::endl;

    std::cout << "Loading " << m_Scene->mNumMeshes << " meshes..." << std::endl;

    // Structure to hold meshes that need fixing
    struct PendingFix {
        unsigned int baseVertex;
        unsigned int count;
        std::string name;
    };
    std::vector<PendingFix> itemsToFix;

    // --- LOAD ALL MESHES ---
    for (unsigned int meshIndex = 0; meshIndex < m_Scene->mNumMeshes; meshIndex++) {
        aiMesh* mesh = m_Scene->mMeshes[meshIndex];
        unsigned int baseVertex = vertices.size();
        std::string meshName = mesh->mName.C_Str();

        // 1. Load Vertices
        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex;
            vertex.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

            if (mesh->HasNormals()) {
                vertex.Normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
            }
            if (mesh->mTextureCoords[0]) {
                vertex.TexCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
            }
            vertices.push_back(vertex);
        }

        // 2. Load Indices
        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                indices.push_back(baseVertex + face.mIndices[j]);
            }
        }

        // 3. Load Bones (Standard Method)
        ExtractBoneWeightForVertices(vertices, mesh, m_Scene, baseVertex);

        // 4. DETECT ITEMS TO FIX (Hats, Helmets, etc.)
        // We look for specific keywords in the mesh name
        if (meshName.find("Hat") != std::string::npos ||
            meshName.find("Helmet") != std::string::npos ||
            meshName.find("Helm") != std::string::npos ||
            meshName.find("Cap") != std::string::npos) {

            std::cout << "📝 Found Attachment (" << meshName << ") - Queuing for deferred fix..." << std::endl;
            itemsToFix.push_back({ baseVertex, mesh->mNumVertices, meshName });
        }
    }

    // ======================= 🔧 DEFERRED FIX PASS (SIMPLIFIED) 🔧 =======================
    for (const auto& fix : itemsToFix) {
        std::cout << "🔧 Applying Simple Fix to: " << fix.name << "..." << std::endl;

        int headBoneID = -1;

        // Find Head Bone ID
        if (m_BoneInfoMap.find("head") != m_BoneInfoMap.end()) headBoneID = m_BoneInfoMap["head"].id;
        else if (m_BoneInfoMap.find("Head") != m_BoneInfoMap.end()) headBoneID = m_BoneInfoMap["Head"].id;

        if (headBoneID != -1) {

            // 🚫 REMOVED: 'toModelSpace' calculation.
            // We assume the geometry is already at the correct height in the file.

            // 🛠️ MANUAL ADJUSTMENT ONLY 🛠️
            // Use this ONLY for small tweaks (e.g. if it clips into the skull)
            float manualY = 0.0f;

            if (fix.name.find("Hat") != std::string::npos) {
                manualY = 0.5f; // Small tweak for Mage Hat
            }
            else if (fix.name.find("Helmet") != std::string::npos) {
                manualY = 0.0f;   // Warrior Helmet likely needs no change now
            }

            glm::mat4 heightCorrection = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, manualY, 0.0f));

            // Apply ONLY the manual tweak (if any)
            for (size_t v = fix.baseVertex; v < fix.baseVertex + fix.count; v++) {

                // 1. Apply small manual offset
                glm::vec4 pos = glm::vec4(vertices[v].Position, 1.0f);
                vertices[v].Position = glm::vec3(heightCorrection * pos);
                // vertices[v].Normal = ... (No rotation, so normals usually fine to leave alone for pure translation)

                // 2. FORCE BINDING (This was the real fix all along)
                for (int i = 0; i < 4; i++) {
                    vertices[v].m_BoneIDs[i] = -1;
                    vertices[v].m_Weights[i] = 0.0f;
                }
                vertices[v].m_BoneIDs[0] = headBoneID;
                vertices[v].m_Weights[0] = 1.0f;
            }
            std::cout << "✅ SUCCESS: " << fix.name << " rigidly attached to Head ID: " << headBoneID << std::endl;
        }
        else {
            std::cerr << "❌ ERROR: Head bone not found for " << fix.name << std::endl;
        }
    }

    m_FinalBoneMatrices.resize(250, glm::mat4(1.0f));
    UpdateAnimation(0.0f);
    setupMesh();
}

SkinnedMesh::~SkinnedMesh() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

void SkinnedMesh::ExtractBoneWeightForVertices(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene, unsigned int baseVertex) {
    for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
        int boneID = -1;
        std::string boneName = mesh->mBones[boneIndex]->mName.C_Str();

        if (m_BoneInfoMap.find(boneName) == m_BoneInfoMap.end()) {
            BoneInfo newBoneInfo;
            newBoneInfo.id = m_BoneCounter;
            newBoneInfo.offset = ConvertMatrix(mesh->mBones[boneIndex]->mOffsetMatrix);
            m_BoneInfoMap[boneName] = newBoneInfo;
            boneID = m_BoneCounter++;
        }
        else {
            boneID = m_BoneInfoMap[boneName].id;
        }

        auto weights = mesh->mBones[boneIndex]->mWeights;
        int numWeights = mesh->mBones[boneIndex]->mNumWeights;

        for (int weightIndex = 0; weightIndex < numWeights; ++weightIndex) {
            int vertexId = baseVertex + weights[weightIndex].mVertexId;
            float weight = weights[weightIndex].mWeight;
            SetVertexBoneData(vertices[vertexId], boneID, weight);
        }
    }
}

void SkinnedMesh::SetVertexBoneData(Vertex& vertex, int boneID, float weight) {
    for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
        if (vertex.m_BoneIDs[i] < 0) {
            vertex.m_Weights[i] = weight;
            vertex.m_BoneIDs[i] = boneID;
            break;
        }
    }
}

void SkinnedMesh::setupMesh() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(3, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, m_BoneIDs));

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, m_Weights));

    glBindVertexArray(0);
}

// ✅ ANIMATION HELPERS
const aiNodeAnim* SkinnedMesh::FindNodeAnim(const aiAnimation* animation, const std::string& nodeName) {
    for (unsigned int i = 0; i < animation->mNumChannels; i++) {
        const aiNodeAnim* nodeAnim = animation->mChannels[i];
        if (std::string(nodeAnim->mNodeName.data) == nodeName) {
            return nodeAnim;
        }
    }
    return nullptr;
}

glm::vec3 SkinnedMesh::InterpolatePosition(float animationTime, const aiNodeAnim* nodeAnim) {
    if (nodeAnim->mNumPositionKeys == 1) {
        return glm::vec3(nodeAnim->mPositionKeys[0].mValue.x, nodeAnim->mPositionKeys[0].mValue.y, nodeAnim->mPositionKeys[0].mValue.z);
    }

    unsigned int index = 0;
    for (unsigned int i = 0; i < nodeAnim->mNumPositionKeys - 1; i++) {
        if (animationTime < (float)nodeAnim->mPositionKeys[i + 1].mTime) {
            index = i;
            break;
        }
    }

    float t1 = (float)nodeAnim->mPositionKeys[index].mTime;
    float t2 = (float)nodeAnim->mPositionKeys[index + 1].mTime;
    float factor = (animationTime - t1) / (t2 - t1);
    const aiVector3D& start = nodeAnim->mPositionKeys[index].mValue;
    const aiVector3D& end = nodeAnim->mPositionKeys[index + 1].mValue;
    return glm::mix(glm::vec3(start.x, start.y, start.z), glm::vec3(end.x, end.y, end.z), factor);
}

glm::quat SkinnedMesh::InterpolateRotation(float animationTime, const aiNodeAnim* nodeAnim) {
    if (nodeAnim->mNumRotationKeys == 1) {
        return glm::quat(nodeAnim->mRotationKeys[0].mValue.w, nodeAnim->mRotationKeys[0].mValue.x, nodeAnim->mRotationKeys[0].mValue.y, nodeAnim->mRotationKeys[0].mValue.z);
    }

    unsigned int index = 0;
    for (unsigned int i = 0; i < nodeAnim->mNumRotationKeys - 1; i++) {
        if (animationTime < (float)nodeAnim->mRotationKeys[i + 1].mTime) {
            index = i;
            break;
        }
    }

    float t1 = (float)nodeAnim->mRotationKeys[index].mTime;
    float t2 = (float)nodeAnim->mRotationKeys[index + 1].mTime;
    float factor = (animationTime - t1) / (t2 - t1);
    const aiQuaternion& start = nodeAnim->mRotationKeys[index].mValue;
    const aiQuaternion& end = nodeAnim->mRotationKeys[index + 1].mValue;
    return glm::slerp(glm::quat(start.w, start.x, start.y, start.z), glm::quat(end.w, end.x, end.y, end.z), factor);
}

glm::vec3 SkinnedMesh::InterpolateScale(float animationTime, const aiNodeAnim* nodeAnim) {
    if (nodeAnim->mNumScalingKeys == 1) {
        return glm::vec3(nodeAnim->mScalingKeys[0].mValue.x, nodeAnim->mScalingKeys[0].mValue.y, nodeAnim->mScalingKeys[0].mValue.z);
    }

    unsigned int index = 0;
    for (unsigned int i = 0; i < nodeAnim->mNumScalingKeys - 1; i++) {
        if (animationTime < (float)nodeAnim->mScalingKeys[i + 1].mTime) {
            index = i;
            break;
        }
    }

    float t1 = (float)nodeAnim->mScalingKeys[index].mTime;
    float t2 = (float)nodeAnim->mScalingKeys[index + 1].mTime;
    float factor = (animationTime - t1) / (t2 - t1);
    const aiVector3D& start = nodeAnim->mScalingKeys[index].mValue;
    const aiVector3D& end = nodeAnim->mScalingKeys[index + 1].mValue;
    return glm::mix(glm::vec3(start.x, start.y, start.z), glm::vec3(end.x, end.y, end.z), factor);
}

// ✅ ANIMATION: Traverse skeleton hierarchy
void SkinnedMesh::ReadNodeHierarchy(float animationTime, const aiNode* pNode, const glm::mat4& parentTransform) {
    std::string nodeName(pNode->mName.data);
    glm::mat4 nodeTransform = ConvertMatrix(pNode->mTransformation);

    // Apply Animation if exists
    if (m_Scene->HasAnimations()) {
        const aiAnimation* animation = m_Scene->mAnimations[0];
        const aiNodeAnim* nodeAnim = FindNodeAnim(animation, nodeName);

        // Mixamo Prefix Fix
        if (!nodeAnim) {
            std::string cleanName = nodeName;
            size_t colonPos = cleanName.find(':');
            if (colonPos != std::string::npos) {
                cleanName = cleanName.substr(colonPos + 1);
                nodeAnim = FindNodeAnim(animation, cleanName);
            }
        }

        if (nodeAnim) {
            glm::vec3 position = InterpolatePosition(animationTime, nodeAnim);
            glm::quat rotation = InterpolateRotation(animationTime, nodeAnim);
            glm::vec3 scale = InterpolateScale(animationTime, nodeAnim);

            glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
            glm::mat4 rotationMat = glm::mat4_cast(rotation);
            glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);

            nodeTransform = translation * rotationMat * scaleMat;
        }
    }

    // ======================= 🦒 NECK EXTENDER FIX 🦒 =======================
    // This manually moves the head bone UP, separating it from the torso.
    if (nodeName == "head" || nodeName == "Head") {
        float neckHeight = 1.25f; // Value from your previous setting

        // Translate UP on the Y-axis relative to the parent bone
        nodeTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, neckHeight, 0.0f)) * nodeTransform;
    }
    // =======================================================================

    glm::mat4 globalTransform = parentTransform * nodeTransform;

    if (m_BoneInfoMap.find(nodeName) != m_BoneInfoMap.end()) {
        int boneIndex = m_BoneInfoMap[nodeName].id;
        if (boneIndex < m_FinalBoneMatrices.size()) {
            glm::mat4 offset = m_BoneInfoMap[nodeName].offset;
            m_FinalBoneMatrices[boneIndex] = m_GlobalInverseTransform * globalTransform * offset;
        }
    }

    for (unsigned int i = 0; i < pNode->mNumChildren; i++) {
        ReadNodeHierarchy(animationTime, pNode->mChildren[i], globalTransform);
    }
}

void SkinnedMesh::UpdateAnimation(float timeInSeconds) {
    ReadNodeHierarchy(0.0f, m_Scene->mRootNode, glm::mat4(1.0f));

    if (!m_Scene->HasAnimations()) return;

    const aiAnimation* animation = m_Scene->mAnimations[0];
    float ticksPerSecond = (float)(animation->mTicksPerSecond != 0 ? animation->mTicksPerSecond : 25.0f);
    float timeInTicks = timeInSeconds * ticksPerSecond;
    float animationTime = fmod(timeInTicks, (float)animation->mDuration);

    ReadNodeHierarchy(animationTime, m_Scene->mRootNode, glm::mat4(1.0f));
}

void SkinnedMesh::Draw(GLuint shaderProgram) {
    if (indices.empty()) return;
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}