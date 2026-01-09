#pragma once
#include <memory>

class IScene;

std::unique_ptr<IScene> CreateDemoScene();
std::unique_ptr<IScene> CreateDebugScene();
