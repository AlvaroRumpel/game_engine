#pragma once
#include <memory>
#include <cstdint>

struct Transform
{
    float x = 0.0f;
    float y = 0.0f;
};

struct RectRender
{
    int w = 32;
    int h = 32;
    unsigned char r = 255, g = 255, b = 255, a = 255;
    bool enabled = true; // debug/fallback
};

class Texture;

struct SpriteRender
{
    std::shared_ptr<Texture> texture;
    float scale = 1.0f;
    bool enabled = false;
};

struct ColliderAABB
{
    float w = 32.0f;
    float h = 32.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    bool isTrigger = false;
    bool enabled = false;
    std::uint32_t layerMask = 0xFFFFFFFFu;
};

struct RigidBody2D
{
    float vx = 0.0f;
    float vy = 0.0f;
    bool isKinematic = false;
    bool enabled = false;
};

struct Entity
{
    int id = 0;
    bool isSolid = true;

    int renderLayer = 0;

    Transform transform;
    RectRender rect;
    SpriteRender sprite;
    ColliderAABB collider;
    RigidBody2D rigidbody;
};
