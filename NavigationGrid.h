#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <iostream>

class NavigationGrid {
private:
    int m_Width, m_Height;

    // The grid: true = BLOCKED, false = WALKABLE
    std::vector<bool> m_Grid;

public:
    NavigationGrid(int width, int height) : m_Width(width), m_Height(height) {
        // Initialize entire map as walkable (false)
        m_Grid.resize(width * height, false);
    }

    // Helper: 2D Index to 1D Index
    int getIndex(int x, int z) const {
        if (x < 0 || x >= m_Width || z < 0 || z >= m_Height) return -1;
        return z * m_Width + x;
    }

    // Check if a tile is blocked
    bool isBlocked(int x, int z) const {
        int idx = getIndex(x, z);
        if (idx == -1) return true; // Out of bounds is blocked
        return m_Grid[idx];
    }

    // Mark a specific spot as Blocked (true) or Walkable (false)
    void setBlocked(int x, int z, bool blocked) {
        int idx = getIndex(x, z);
        if (idx != -1) m_Grid[idx] = blocked;
    }

    // Mark a circle area (For buildings, explosions, trees)
    void updateArea(glm::vec3 center, float radius, bool blocked) {
        int gridX = (int)center.x;
        int gridZ = (int)center.z;
        int r = (int)ceil(radius);

        // Loop only through the square bounding box of the circle
        for (int x = gridX - r; x <= gridX + r; x++) {
            for (int z = gridZ - r; z <= gridZ + r; z++) {
                // Precise circle check
                if (glm::distance(glm::vec2(x, z), glm::vec2(gridX, gridZ)) <= radius) {
                    setBlocked(x, z, blocked);
                }
            }
        }
    }

    // Clear everything (Reset map)
    void clear() {
        std::fill(m_Grid.begin(), m_Grid.end(), false);
    }
};