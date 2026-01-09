#pragma once

struct Camera2D
{
    float x = 0.0f; // centro da câmera no mundo
    float y = 0.0f;
    float zoom = 1.0f; // 1.0 = normal, 2.0 = aproxima, 0.5 = afasta
};
