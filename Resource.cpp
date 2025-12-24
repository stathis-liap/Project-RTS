// Resources.cpp
#include "Resource.h"

Resources::Resources() : wood(0), food(0) {}

void Resources::addWood(int amount) {
    wood += amount;
}

void Resources::addFood(int amount) {
    food += amount;
}

int Resources::getWood() const {
    return wood;
}

int Resources::getFood() const {
    return food;
}