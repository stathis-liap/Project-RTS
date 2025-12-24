// Resources.h
#ifndef RESOURCES_H
#define RESOURCES_H

class Resources {
public:
    int wood;
    int food;

    Resources();

    void addWood(int amount);
    void addFood(int amount);

    int getWood() const;
    int getFood() const;
};

#endif // RESOURCES_H