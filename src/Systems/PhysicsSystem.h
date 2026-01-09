#pragma once
#include <cstdint>
#include <unordered_set>

class Engine;
class Scene;
class IScene;

struct PhysicsStats
{
    int pairsTested = 0;
    int collisions = 0;
    int activePairs = 0;
};

class PhysicsSystem
{
public:
    void setCellSize(int size) { cellSize_ = size; }
    int cellSize() const { return cellSize_; }

    void step(Engine &engine, Scene &scene, float fixedDt, IScene *callbacks);
    void debugRender(Engine &engine, const Scene &scene);
    void reset();

    const PhysicsStats &stats() const { return stats_; }

private:
    std::uint64_t pairKey(int a, int b) const;
    void resolve(Engine &engine, Scene &scene, int idA, int idB);

private:
    int cellSize_ = 64;
    std::unordered_set<std::uint64_t> prevPairs_;
    PhysicsStats stats_;
};
