#include "Engine/Engine.h"
#include <cstdio>
#include <memory>

class DebugScene;

std::unique_ptr<IScene> CreateDemoScene();
std::unique_ptr<IScene> CreateDebugScene();

static void FillDemoMap(Tilemap &map)
{
  for (int y = 0; y < map.height; ++y)
  {
    for (int x = 0; x < map.width; ++x)
    {
      int tileId = 0;
      bool border = (x == 0 || y == 0 || x == map.width - 1 || y == map.height - 1);
      if (border)
        tileId = 3;
      else if ((x % 9) == 0 || (y % 9) == 0)
        tileId = 2;
      else if (((x + y) % 11) == 0)
        tileId = 1;
      map.set(x, y, tileId);
    }
  }
}

static void FillDebugMap(Tilemap &map)
{
  for (int y = 0; y < map.height; ++y)
  {
    for (int x = 0; x < map.width; ++x)
    {
      int tileId = ((x + y) % 5);
      if (x % 7 == 0 || y % 7 == 0)
        tileId = 4;
      map.set(x, y, tileId);
    }
  }
}

static int CreateObstacle(Engine &engine, float x, float y, int w, int h, unsigned char r, unsigned char g, unsigned char b)
{
  auto &e = engine.scene().createEntity();
  e.transform.x = x;
  e.transform.y = y;
  e.rect.w = w;
  e.rect.h = h;
  e.rect.r = r;
  e.rect.g = g;
  e.rect.b = b;
  e.rect.a = 255;
  e.rect.enabled = true;
  e.sprite.enabled = false;
  e.collider.enabled = true;
  e.collider.w = (float)w;
  e.collider.h = (float)h;
  e.rigidbody.enabled = true;
  e.rigidbody.isKinematic = true;
  return e.id;
}

static void DrawHud(Engine &engine, const Font &font, const char *sceneName)
{
  const Time &time = engine.time();
  const RenderStats &stats = engine.commandBuffer().stats();
  const Camera2D &cam = engine.camera();
  const PhysicsStats &physics = engine.physicsStats();

  float ms = time.deltaTime() * 1000.0f;
  float fps = time.fps();

  char line1[256];
  char line2[256];
  char line3[256];
  char line4[256];
  char line5[256];
  char line6[256];
  std::snprintf(line1, sizeof(line1), "[%s] FPS: %.1f  Frame: %.2f ms", sceneName, fps, ms);
  std::snprintf(line2, sizeof(line2), "Draws: %u  Sprites: %u  Batches: %u",
                stats.rectDraws + stats.spriteDraws, stats.spriteDraws, stats.spriteBatches);

  float wx = 0.0f;
  float wy = 0.0f;
  engine.screenToWorld((float)engine.input().mouseX(), (float)engine.input().mouseY(), wx, wy);
  std::snprintf(line3, sizeof(line3), "Mouse: %d,%d  World: %.1f,%.1f",
                engine.input().mouseX(), engine.input().mouseY(), wx, wy);
  std::snprintf(line4, sizeof(line4), "Camera: %.1f,%.1f  Zoom: %.2f", cam.x, cam.y, cam.zoom);
  std::snprintf(line5, sizeof(line5), "Collisions: %d  ActivePairs: %d", physics.collisions, physics.activePairs);
  std::snprintf(line6, sizeof(line6), "Manifest: %s", engine.assets().manifestLoaded() ? "OK" : "MISSING");

  engine.renderer().drawText(font, line1, 10, 10, 255, 255, 255, 255);
  engine.renderer().drawText(font, line2, 10, 32, 200, 200, 200, 255);
  engine.renderer().drawText(font, line3, 10, 54, 200, 200, 200, 255);
  engine.renderer().drawText(font, line4, 10, 76, 200, 200, 200, 255);
  engine.renderer().drawText(font, line5, 10, 98, 200, 200, 200, 255);
  engine.renderer().drawText(font, line6, 10, 120, 200, 200, 200, 255);
}

class DemoScene : public IScene
{
public:
  void onEnter(Engine &engine) override
  {
    Tilemap &map = engine.scene().createTilemap(80, 80, 32);
    FillDemoMap(map);

    auto &e = engine.scene().createEntity();
    e.transform.x = 100;
    e.transform.y = 100;

    font_ = engine.assets().loadFontById("ui_font");

    e.sprite.texture = engine.assets().loadTextureById("player");
    e.sprite.enabled = true;
    e.sprite.scale = 1.0f;

    // desligar rect para nao desenhar quadrado
    e.rect.enabled = false;

    e.collider.enabled = true;
    e.collider.w = 28.0f;
    e.collider.h = 28.0f;
    e.rigidbody.enabled = true;

    playerId_ = e.id;

    CreateObstacle(engine, 300, 220, 120, 40, 80, 80, 200);
    CreateObstacle(engine, 500, 380, 40, 160, 80, 200, 120);
    CreateObstacle(engine, 200, 480, 180, 30, 200, 120, 80);
  }

  void onUpdate(Engine &engine, float dt) override
  {
    auto &cam = engine.camera();
    Entity *p = engine.scene().findEntity(playerId_);
    if (!p)
      return;

    Input &input = engine.input();

    if (input.pressed("ToggleDebug"))
      engine.setScene(CreateDebugScene());
    if (input.pressed("ToggleCollision"))
      engine.setPhysicsDebugDraw(!engine.physicsDebugDraw());

    bool zoomInDown = input.down("ZoomIn");
    bool zoomOutDown = input.down("ZoomOut");
    if (zoomInDown)
      cam.zoom *= 1.0f + 1.5f * dt;
    if (zoomOutDown)
      cam.zoom *= 1.0f - 1.5f * dt;

    if (!zoomInDown && input.pressed("ZoomIn"))
      cam.zoom *= 1.10f;
    if (!zoomOutDown && input.pressed("ZoomOut"))
      cam.zoom *= 0.90f;

    cam.x = p->transform.x;
    cam.y = p->transform.y;
    if (cam.zoom < 0.1f)
      cam.zoom = 0.1f;
    if (cam.zoom > 6.0f)
      cam.zoom = 6.0f;
  }

  void onFixedUpdate(Engine &engine, float fixedDt) override
  {
    (void)fixedDt;
    Entity *p = engine.scene().findEntity(playerId_);
    if (!p)
      return;

    Input &input = engine.input();
    float x = input.getAxis("MoveX");
    float y = input.getAxis("MoveY");

    float speed = 220.0f;
    p->rigidbody.vx = x * speed;
    p->rigidbody.vy = y * speed;
  }

  void onRenderUI(Engine &engine) override
  {
    if (!font_)
      return;
    DrawHud(engine, *font_, "DemoScene");
  }

private:
  int playerId_ = 0;
  std::shared_ptr<Font> font_;
};

class DebugScene : public IScene
{
public:
  void onEnter(Engine &engine) override
  {
    Tilemap &map = engine.scene().createTilemap(60, 60, 40);
    FillDebugMap(map);

    auto &e = engine.scene().createEntity();
    e.transform.x = 100;
    e.transform.y = 100;

    font_ = engine.assets().loadFontById("ui_font");

    e.rect.w = 40;
    e.rect.h = 40;
    e.rect.r = 80;
    e.rect.g = 200;
    e.rect.b = 255;
    e.rect.a = 255;
    e.rect.enabled = true;

    e.sprite.enabled = false;

    e.collider.enabled = true;
    e.collider.w = 40.0f;
    e.collider.h = 40.0f;
    e.rigidbody.enabled = true;

    playerId_ = e.id;

    CreateObstacle(engine, 260, 180, 80, 80, 200, 80, 120);
    CreateObstacle(engine, 460, 260, 140, 30, 120, 80, 200);
    CreateObstacle(engine, 140, 360, 40, 160, 80, 160, 220);
  }

  void onUpdate(Engine &engine, float dt) override
  {
    auto &cam = engine.camera();
    Entity *p = engine.scene().findEntity(playerId_);
    if (!p)
      return;

    Input &input = engine.input();

    if (input.pressed("ToggleDebug"))
      engine.setScene(CreateDemoScene());
    if (input.pressed("ToggleCollision"))
      engine.setPhysicsDebugDraw(!engine.physicsDebugDraw());

    bool zoomInDown = input.down("ZoomIn");
    bool zoomOutDown = input.down("ZoomOut");
    if (zoomInDown)
      cam.zoom *= 1.0f + 1.5f * dt;
    if (zoomOutDown)
      cam.zoom *= 1.0f - 1.5f * dt;

    if (!zoomInDown && input.pressed("ZoomIn"))
      cam.zoom *= 1.10f;
    if (!zoomOutDown && input.pressed("ZoomOut"))
      cam.zoom *= 0.90f;

    cam.x = p->transform.x;
    cam.y = p->transform.y;
    if (cam.zoom < 0.1f)
      cam.zoom = 0.1f;
    if (cam.zoom > 6.0f)
      cam.zoom = 6.0f;
  }

  void onFixedUpdate(Engine &engine, float fixedDt) override
  {
    (void)fixedDt;
    Entity *p = engine.scene().findEntity(playerId_);
    if (!p)
      return;

    Input &input = engine.input();
    float x = input.getAxis("MoveX");
    float y = input.getAxis("MoveY");

    float speed = 200.0f;
    p->rigidbody.vx = x * speed;
    p->rigidbody.vy = y * speed;
  }

  void onRenderUI(Engine &engine) override
  {
    if (!font_)
      return;
    DrawHud(engine, *font_, "DebugScene");
  }

private:
  int playerId_ = 0;
  std::shared_ptr<Font> font_;
};

std::unique_ptr<IScene> CreateDemoScene()
{
  return std::make_unique<DemoScene>();
}

std::unique_ptr<IScene> CreateDebugScene()
{
  return std::make_unique<DebugScene>();
}

int main()
{
  Engine engine;
  engine.run(CreateDemoScene());
  return 0;
}
