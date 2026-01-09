#pragma once
#include <vector>
#include "Entity.h"

class Scene
{
public:
    Entity &createEntity();
    Entity *findEntity(int id);
    const std::vector<Entity> &entities() const { return entities_; }

    void clear();

private:
    std::vector<Entity> entities_;
    int nextEntityId_ = 1;
};
