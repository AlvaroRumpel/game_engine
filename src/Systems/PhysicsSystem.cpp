#include "PhysicsSystem.h"
#include "../Engine/Engine.h"
#include "../World/Scene.h"
#include "../World/Entity.h"
#include "../World/IScene.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

struct AABB
{
    float minX;
    float minY;
    float maxX;
    float maxY;
};

struct ColliderEntry
{
    Entity *entity = nullptr;
    AABB bounds{};
};

static AABB BuildAABB(const Entity &e)
{
    float x = e.transform.x + e.collider.offsetX;
    float y = e.transform.y + e.collider.offsetY;
    return {x, y, x + e.collider.w, y + e.collider.h};
}

std::uint64_t PhysicsSystem::pairKey(int a, int b) const
{
    std::uint32_t minId = (a < b) ? (std::uint32_t)a : (std::uint32_t)b;
    std::uint32_t maxId = (a < b) ? (std::uint32_t)b : (std::uint32_t)a;
    return (static_cast<std::uint64_t>(minId) << 32) | maxId;
}

static bool Intersects(const AABB &a, const AABB &b)
{
    return !(a.maxX <= b.minX || a.minX >= b.maxX || a.maxY <= b.minY || a.minY >= b.maxY);
}

static float OverlapAmount(float amin, float amax, float bmin, float bmax)
{
    return std::min(amax, bmax) - std::max(amin, bmin);
}

void PhysicsSystem::resolve(Engine &engine, Scene &scene, int idA, int idB)
{
    (void)engine;
    Entity *a = scene.findEntity(idA);
    Entity *b = scene.findEntity(idB);
    if (!a || !b)
        return;

    if (a->collider.isTrigger || b->collider.isTrigger)
        return;

    bool aKinematic = a->rigidbody.enabled && a->rigidbody.isKinematic;
    bool bKinematic = b->rigidbody.enabled && b->rigidbody.isKinematic;
    if (aKinematic && bKinematic)
        return;

    AABB ab = BuildAABB(*a);
    AABB bb = BuildAABB(*b);

    float overlapX = OverlapAmount(ab.minX, ab.maxX, bb.minX, bb.maxX);
    float overlapY = OverlapAmount(ab.minY, ab.maxY, bb.minY, bb.maxY);
    if (overlapX <= 0.0f || overlapY <= 0.0f)
        return;

    float axCenter = (ab.minX + ab.maxX) * 0.5f;
    float ayCenter = (ab.minY + ab.maxY) * 0.5f;
    float bxCenter = (bb.minX + bb.maxX) * 0.5f;
    float byCenter = (bb.minY + bb.maxY) * 0.5f;

    float moveAx = 0.0f;
    float moveAy = 0.0f;
    float moveBx = 0.0f;
    float moveBy = 0.0f;

    if (overlapX < overlapY)
    {
        float dir = (axCenter < bxCenter) ? -1.0f : 1.0f;
        if (aKinematic)
            moveBx = -dir * overlapX;
        else if (bKinematic)
            moveAx = dir * overlapX;
        else
        {
            moveAx = dir * (overlapX * 0.5f);
            moveBx = -dir * (overlapX * 0.5f);
        }
    }
    else
    {
        float dir = (ayCenter < byCenter) ? -1.0f : 1.0f;
        if (aKinematic)
            moveBy = -dir * overlapY;
        else if (bKinematic)
            moveAy = dir * overlapY;
        else
        {
            moveAy = dir * (overlapY * 0.5f);
            moveBy = -dir * (overlapY * 0.5f);
        }
    }

    a->transform.x += moveAx;
    a->transform.y += moveAy;
    b->transform.x += moveBx;
    b->transform.y += moveBy;
}

void PhysicsSystem::step(Engine &engine, Scene &scene, float fixedDt, IScene *callbacks)
{
    stats_ = PhysicsStats{};

    // Integrate velocities
    for (auto &e : scene.entities())
    {
        if (!e.rigidbody.enabled || e.rigidbody.isKinematic)
            continue;
        e.transform.x += e.rigidbody.vx * fixedDt;
        e.transform.y += e.rigidbody.vy * fixedDt;
    }

    std::vector<ColliderEntry> colliders;
    colliders.reserve(scene.entities().size());

    for (auto &e : scene.entities())
    {
        if (!e.collider.enabled)
            continue;
        ColliderEntry entry;
        entry.entity = &e;
        entry.bounds = BuildAABB(e);
        colliders.push_back(entry);
    }

    std::unordered_map<std::int64_t, std::vector<int>> grid;
    grid.reserve(colliders.size() * 2);

    float invCell = 1.0f / (float)cellSize_;
    for (int i = 0; i < (int)colliders.size(); ++i)
    {
        const AABB &b = colliders[i].bounds;
        int minCx = (int)std::floor(b.minX * invCell);
        int maxCx = (int)std::floor(b.maxX * invCell);
        int minCy = (int)std::floor(b.minY * invCell);
        int maxCy = (int)std::floor(b.maxY * invCell);
        for (int cy = minCy; cy <= maxCy; ++cy)
        {
            for (int cx = minCx; cx <= maxCx; ++cx)
            {
                std::int64_t key = (static_cast<std::int64_t>(cx) << 32) ^ (std::uint32_t)cy;
                grid[key].push_back(i);
            }
        }
    }

    std::unordered_set<std::uint64_t> newPairs;
    newPairs.reserve(prevPairs_.size() + 16);

    for (const auto &cell : grid)
    {
        const std::vector<int> &items = cell.second;
        for (size_t i = 0; i < items.size(); ++i)
        {
            for (size_t j = i + 1; j < items.size(); ++j)
            {
                ColliderEntry &a = colliders[items[i]];
                ColliderEntry &b = colliders[items[j]];

                int idA = a.entity->id;
                int idB = b.entity->id;
                if (idA == idB)
                    continue;

                if ((a.entity->collider.layerMask & b.entity->collider.layerMask) == 0)
                    continue;

                stats_.pairsTested++;
                if (!Intersects(a.bounds, b.bounds))
                    continue;

                stats_.collisions++;
                std::uint64_t key = pairKey(idA, idB);
                newPairs.insert(key);

                if (prevPairs_.find(key) == prevPairs_.end())
                {
                    if (callbacks)
                        callbacks->onCollisionEnter(engine, idA, idB);
                }
                else
                {
                    if (callbacks)
                        callbacks->onCollisionStay(engine, idA, idB);
                }

                resolve(engine, scene, idA, idB);
            }
        }
    }

    for (auto key : prevPairs_)
    {
        if (newPairs.find(key) != newPairs.end())
            continue;
        int idA = (int)(key >> 32);
        int idB = (int)(key & 0xFFFFFFFFu);
        if (callbacks)
            callbacks->onCollisionExit(engine, idA, idB);
    }

    prevPairs_.swap(newPairs);
    stats_.activePairs = (int)prevPairs_.size();
}

void PhysicsSystem::debugRender(Engine &engine, const Scene &scene)
{
    auto &q = engine.renderQueue();

    for (const auto &e : scene.entities())
    {
        if (!e.collider.enabled)
            continue;

        RenderCommand cmd;
        cmd.type = RenderCommandType::Rect;
        cmd.layer = 100;
        cmd.x = e.transform.x + e.collider.offsetX;
        cmd.y = e.transform.y + e.collider.offsetY;
        cmd.w = (int)e.collider.w;
        cmd.h = (int)e.collider.h;
        if (e.collider.isTrigger)
        {
            cmd.r = 255;
            cmd.g = 200;
            cmd.b = 80;
            cmd.a = 120;
        }
        else
        {
            cmd.r = 220;
            cmd.g = 80;
            cmd.b = 80;
            cmd.a = 120;
        }
        q.submit(cmd);
    }
}

void PhysicsSystem::reset()
{
    prevPairs_.clear();
    stats_ = PhysicsStats{};
}
