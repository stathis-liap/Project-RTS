#include "Resource.h"
#include <iostream>

Resources::Resources() : wood(500), rock(500) {} // Start with some resources for testing

void Resources::addWood(int amount) { wood += amount; }
void Resources::addRock(int amount) { rock += amount; }

int Resources::getWood() const { return wood; }
int Resources::getRock() const { return rock; }

//  Check if player has enough
bool Resources::canAfford(int woodCost, int rockCost) const {
    return (wood >= woodCost && rock >= rockCost);
}

//  Deduct resources
bool Resources::spend(int woodCost, int rockCost) {
    if (!canAfford(woodCost, rockCost)) {
        std::cout << "Not enough resources!" << std::endl;
        return false;
    }
    wood -= woodCost;
    rock -= rockCost;
    std::cout << "Spent " << woodCost << " Wood, " << rockCost << " Rock." << std::endl;
    return true;
}