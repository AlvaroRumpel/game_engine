#pragma once

struct Transform
{
    float x = 0.0f;
    float y = 0.0f;
};

struct RectRender
{
    int w = 32;
    int h = 32;
    unsigned char r = 255;
    unsigned char g = 255;
    unsigned char b = 255;
    unsigned char a = 255;
};

struct Velocity
{
    float vx = 0.0f; // pixels/segundo
    float vy = 0.0f;
};
