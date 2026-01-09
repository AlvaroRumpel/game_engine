#pragma once
#include <vector>
#include "Entity.h"
#include "Tilemap.h"

class Scene
{
public:
    struct Bounds
    {
        bool enabled = false;
        float x = 0.0f;
        float y = 0.0f;
        float w = 2000.0f;
        float h = 2000.0f;
    };

    Entity &createEntity();
    Entity *findEntity(int id);
    std::vector<Entity> &entities() { return entities_; }
    const std::vector<Entity> &entities() const { return entities_; }
    bool destroyEntity(int id);
    void clearEntities();
    Bounds &bounds() { return bounds_; }
    const Bounds &bounds() const { return bounds_; }

    Tilemap &createTilemap(int width, int height, int tileSize);
    const std::vector<Tilemap> &tilemaps() const { return tilemaps_; }

    void clear();

private:
    std::vector<Entity> entities_;
    int nextEntityId_ = 1;
    std::vector<Tilemap> tilemaps_;
    Bounds bounds_{};
};
