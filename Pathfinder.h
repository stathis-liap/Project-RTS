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
public:
    // Helper: Find nearest walkable tile if target is blocked
    static glm::vec3 findNearestWalkable(int targetX, int targetZ, const NavigationGrid* grid) {
        // Spiral search radius
        int radius = 1;
        int maxRadius = 10; // Don't search forever

        while (radius < maxRadius) {
            for (int x = targetX - radius; x <= targetX + radius; x++) {
                for (int z = targetZ - radius; z <= targetZ + radius; z++) {
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

        // --- DEBUG PRINTS ---
        // Uncomment these if you want to see every click details
        // std::cout << "Pathing request: " << startX << "," << startZ << " -> " << targetX << "," << targetZ << std::endl;

        // 1. CHECK START NODE
        if (grid->isBlocked(startX, startZ)) {
            std::cout << "⚠️ START POINT BLOCKED! Unit is stuck in an obstacle." << std::endl;

            // Try to find a free spot nearby to snap to
            glm::vec3 freeStart = findNearestWalkable(startX, startZ, grid);
            if (freeStart.x != -1.0f) {
                std::cout << " -> Snapped start to: " << freeStart.x << ", " << freeStart.z << std::endl;
                startX = (int)freeStart.x;
                startZ = (int)freeStart.z;
            }
            else {
                std::cout << " -> CRITICAL: No free spot found near unit!" << std::endl;
                return path; // Give up
            }
        }

        // 2. CHECK TARGET NODE
        if (grid->isBlocked(targetX, targetZ)) {
            std::cout << "⚠️ TARGET BLOCKED! Searching for nearest valid spot..." << std::endl;
            glm::vec3 newTarget = findNearestWalkable(targetX, targetZ, grid);
            if (newTarget.x == -1.0f) {
                std::cout << " -> Target is completely inaccessible." << std::endl;
                return path;
            }
            targetX = (int)newTarget.x;
            targetZ = (int)newTarget.z;
        }

        // Standard A* Setup
        std::priority_queue<Node*, std::vector<Node*>, CompareNode> openSet;
        static std::vector<bool> closedSet(512 * 512, false);
        std::fill(closedSet.begin(), closedSet.end(), false);

        std::vector<Node*> allNodes;

        Node* startNode = new Node{ startX, startZ, 0.0f, 0.0f, nullptr };
        startNode->hCost = glm::distance(glm::vec2(startX, startZ), glm::vec2(targetX, targetZ));
        openSet.push(startNode);
        allNodes.push_back(startNode);

        Node* finalNode = nullptr;
        int nodesExplored = 0; // Debug counter

        while (!openSet.empty()) {
            Node* current = openSet.top();
            openSet.pop();
            nodesExplored++;

            int currentIdx = current->z * 512 + current->x;
            if (currentIdx < 0 || currentIdx >= closedSet.size()) continue;
            if (closedSet[currentIdx]) continue;
            closedSet[currentIdx] = true;

            // Found Goal?
            if (abs(current->x - targetX) <= 1 && abs(current->z - targetZ) <= 1) {
                finalNode = current;
                break;
            }

            // Stop if searching too long (performance safety)
            if (nodesExplored > 10000) {
                std::cout << "❌ Pathfinding timeout (too complex)." << std::endl;
                break;
            }

            for (int dx = -1; dx <= 1; dx++) {
                for (int dz = -1; dz <= 1; dz++) {
                    if (dx == 0 && dz == 0) continue;

                    int nx = current->x + dx;
                    int nz = current->z + dz;

                    // Boundary Checks
                    if (nx < 0 || nx >= 512 || nz < 0 || nz >= 512) continue;

                    // Obstacle Check
                    if (grid->isBlocked(nx, nz)) continue;

                    int neighborIdx = nz * 512 + nx;
                    if (closedSet[neighborIdx]) continue;

                    float newGCost = current->gCost + ((dx != 0 && dz != 0) ? 1.414f : 1.0f);

                    Node* neighbor = new Node{ nx, nz, newGCost, 0.0f, current };
                    neighbor->hCost = glm::distance(glm::vec2(nx, nz), glm::vec2(targetX, targetZ));
                    openSet.push(neighbor);
                    allNodes.push_back(neighbor);
                }
            }
        }

        if (finalNode) {
            Node* curr = finalNode;
            while (curr != nullptr) {
                path.push_back(glm::vec3(curr->x, 0.0f, curr->z));
                curr = curr->parent;
            }
            std::reverse(path.begin(), path.end());
            if (!path.empty()) path.erase(path.begin());

            // Debug Success
            // std::cout << "✅ Path found! Steps: " << path.size() << std::endl;
        }
        else {
            std::cout << "❌ No path could be found to target." << std::endl;
        }

        for (Node* n : allNodes) delete n;
        return path;
    }
};