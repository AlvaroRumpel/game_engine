#include "Engine/Engine.h"
#include "Game/SandboxScenes.h"

int main()
{
  Engine engine;
  engine.run(CreateDemoScene());
  return 0;
}