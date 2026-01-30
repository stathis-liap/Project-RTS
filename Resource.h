#ifndef RESOURCE_H
#define RESOURCE_H

class Resources {
public:
    Resources();

    void addWood(int amount);
    void addRock(int amount);
    
    // ✅ NEW: Check if we can afford a cost
    bool canAfford(int woodCost, int rockCost) const;

    // ✅ NEW: Spend resources (returns true if successful)
    bool spend(int woodCost, int rockCost);

    int getWood() const;
    int getRock() const;

private:
    int wood;
    int rock;
};

#endif