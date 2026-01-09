#pragma once
#include <vector>
#include "Entity.h"
#include "Tilemap.h"

class Scene
{
public:
    Entity &createEntity();
    Entity *findEntity(int id);
    std::vector<Entity> &entities() { return entities_; }
    const std::vector<Entity> &entities() const { return entities_; }

    Tilemap &createTilemap(int width, int height, int tileSize);
    const std::vector<Tilemap> &tilemaps() const { return tilemaps_; }

    void clear();

private:
    std::vector<Entity> entities_;
    int nextEntityId_ = 1;
    std::vector<Tilemap> tilemaps_;
};
