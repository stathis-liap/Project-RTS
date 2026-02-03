#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <queue>
#include <cmath>
#include <algorithm>
#include <iostream>
#include "NavigationGrid.h" 

struct Node {
    int x, z;
    float gCost;
    float hCost;
    Node* parent;
    float fCost() const { return gCost + hCost; }
};

struct CompareNode {
    bool operator()(Node* a, Node* b) {
        return a->fCost() > b->fCost();
    }
};

class Pathfinder {
    // ✅ DEFINE BORDER SIZE (Matches your rock border thickness)
    static const int BORDER_SIZE = 35;
    static const int MAP_SIZE = 512;

public:
    // Helper: Find nearest walkable tile if target is blocked
    static glm::vec3 findNearestWalkable(int targetX, int targetZ, const NavigationGrid* grid) {
        int radius = 1;
        int maxRadius = 10;

        while (radius < maxRadius) {
            for (int x = targetX - radius; x <= targetX + radius; x++) {
                for (int z = targetZ - radius; z <= targetZ + radius; z++) {

                    // ✅ CHECK BORDER LIMITS
                    if (x < BORDER_SIZE || x >= MAP_SIZE - BORDER_SIZE ||
                        z < BORDER_SIZE || z >= MAP_SIZE - BORDER_SIZE) continue;

                    if (!grid->isBlocked(x, z)) {
                        return glm::vec3(x, 0.0f, z);
                    }
                }
            }
            radius++;
        }
        return glm::vec3(-1.0f); // Failed
    }

    static std::vector<glm::vec3> findPath(glm::vec3 start, glm::vec3 target, const NavigationGrid* grid)
    {
        std::vector<glm::vec3> path;

        int startX = (int)start.x;
        int startZ = (int)start.z;
        int targetX = (int)target.x;
        int targetZ = (int)target.z;

        // =========================================================
        // ✅ 1. CLAMP TARGET TO SAFE ZONE
        // =========================================================
        // If user clicks on the border rocks (e.g., x=5), force target to x=15
        if (targetX < BORDER_SIZE) targetX = BORDER_SIZE;
        if (targetX >= MAP_SIZE - BORDER_SIZE) targetX = MAP_SIZE - BORDER_SIZE - 1;

        if (targetZ < BORDER_SIZE) targetZ = BORDER_SIZE;
        if (targetZ >= MAP_SIZE - BORDER_SIZE) targetZ = MAP_SIZE - BORDER_SIZE - 1;

        // ---------------------------------------------------------
        // 2. CHECK START NODE
        // ---------------------------------------------------------
        if (grid->isBlocked(startX, startZ)) {
            // std::cout << "⚠️ START POINT BLOCKED! Unit is stuck." << std::endl;
            glm::vec3 freeStart = findNearestWalkable(startX, startZ, grid);
            if (freeStart.x != -1.0f) {
                startX = (int)freeStart.x;
                startZ = (int)freeStart.z;
            }
            else {
                return path; // Give up
            }
        }

        // ---------------------------------------------------------
        // 3. CHECK TARGET NODE
        // ---------------------------------------------------------
        if (grid->isBlocked(targetX, targetZ)) {
            // std::cout << "⚠️ TARGET BLOCKED! Searching nearby..." << std::endl;
            glm::vec3 newTarget = findNearestWalkable(targetX, targetZ, grid);
            if (newTarget.x == -1.0f) {
                return path;
            }
            targetX = (int)newTarget.x;
            targetZ = (int)newTarget.z;
        }

        // Standard A* Setup
        std::priority_queue<Node*, std::vector<Node*>, CompareNode> openSet;

        // Use static to avoid reallocating memory every click
        static std::vector<bool> closedSet(MAP_SIZE * MAP_SIZE, false);
        // Optimization: Use a "visited ID" instead of clearing vector? 
        // For now, fill is safe enough for 512x512
        std::fill(closedSet.begin(), closedSet.end(), false);

        // Track allocated nodes to delete them later
        std::vector<Node*> allNodes;

        Node* startNode = new Node{ startX, startZ, 0.0f, 0.0f, nullptr };
        startNode->hCost = glm::distance(glm::vec2(startX, startZ), glm::vec2(targetX, targetZ));
        openSet.push(startNode);
        allNodes.push_back(startNode);

        Node* finalNode = nullptr;
        int nodesExplored = 0;

        while (!openSet.empty()) {
            Node* current = openSet.top();
            openSet.pop();
            nodesExplored++;

            int currentIdx = current->z * MAP_SIZE + current->x;
            if (currentIdx < 0 || currentIdx >= closedSet.size()) continue;
            if (closedSet[currentIdx]) continue;
            closedSet[currentIdx] = true;

            // Found Goal?
            if (abs(current->x - targetX) <= 1 && abs(current->z - targetZ) <= 1) {
                finalNode = current;
                break;
            }

            // Safety Cutoff
            if (nodesExplored > 15000) {
                // std::cout << "❌ Pathfinding timeout." << std::endl;
                break;
            }

            // Neighbor Loop
            for (int dx = -1; dx <= 1; dx++) {
                for (int dz = -1; dz <= 1; dz++) {
                    if (dx == 0 && dz == 0) continue;

                    int nx = current->x + dx;
                    int nz = current->z + dz;

                    // =====================================================
                    // ✅ 4. STRICT BORDER CHECK
                    // =====================================================
                    // Ignore any neighbor inside the rock border
                    if (nx < BORDER_SIZE || nx >= MAP_SIZE - BORDER_SIZE ||
                        nz < BORDER_SIZE || nz >= MAP_SIZE - BORDER_SIZE) continue;

                    // Standard Obstacle Check
                    if (grid->isBlocked(nx, nz)) continue;

                    int neighborIdx = nz * MAP_SIZE + nx;
                    if (closedSet[neighborIdx]) continue;

                    float newGCost = current->gCost + ((dx != 0 && dz != 0) ? 1.414f : 1.0f);

                    Node* neighbor = new Node{ nx, nz, newGCost, 0.0f, current };
                    neighbor->hCost = glm::distance(glm::vec2(nx, nz), glm::vec2(targetX, targetZ));
                    openSet.push(neighbor);
                    allNodes.push_back(neighbor);
                }
            }
        }

        // Reconstruct Path
        if (finalNode) {
            Node* curr = finalNode;
            while (curr != nullptr) {
                path.push_back(glm::vec3(curr->x, 0.0f, curr->z));
                curr = curr->parent;
            }
            std::reverse(path.begin(), path.end());
            if (!path.empty()) path.erase(path.begin());
        }

        // Cleanup Memory
        for (Node* n : allNodes) delete n;

        return path;
    }
};