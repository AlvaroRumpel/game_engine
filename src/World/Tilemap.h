#pragma once
#include <vector>

struct Tilemap
{
    int width = 0;
    int height = 0;
    int tileSize = 32;
    float originX = 0.0f;
    float originY = 0.0f;
    std::vector<int> tiles;

    bool inBounds(int x, int y) const
    {
        return x >= 0 && y >= 0 && x < width && y < height;
    }

    int get(int x, int y) const
    {
        if (!inBounds(x, y))
            return -1;
        return tiles[(y * width) + x];
    }

    void set(int x, int y, int value)
    {
        if (!inBounds(x, y))
            return;
        tiles[(y * width) + x] = value;
    }
};
