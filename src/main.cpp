#include "Engine/Engine.h"
#include <cstdio>
#include <memory>

class DebugScene;

std::unique_ptr<IScene> CreateDemoScene();
std::unique_ptr<IScene> CreateDebugScene();

static void DrawHud(Engine &engine, const Font &font, const char *sceneName)
{
  const Time &time = engine.time();
  const RenderStats &stats = engine.renderQueue().stats();
  const Camera2D &cam = engine.camera();

  float ms = time.deltaTime() * 1000.0f;
  float fps = time.fps();

  char line1[256];
  char line2[256];
  char line3[256];
  char line4[256];
  std::snprintf(line1, sizeof(line1), "[%s] FPS: %.1f  Frame: %.2f ms", sceneName, fps, ms);
  std::snprintf(line2, sizeof(line2), "Draws: %u  Sprites: %u  Batches: %u",
                stats.rectDraws + stats.spriteDraws, stats.spriteDraws, stats.spriteBatches);

  float wx = 0.0f;
  float wy = 0.0f;
  engine.screenToWorld((float)engine.input().mouseX(), (float)engine.input().mouseY(), wx, wy);
  std::snprintf(line3, sizeof(line3), "Mouse: %d,%d  World: %.1f,%.1f",
                engine.input().mouseX(), engine.input().mouseY(), wx, wy);
  std::snprintf(line4, sizeof(line4), "Camera: %.1f,%.1f  Zoom: %.2f", cam.x, cam.y, cam.zoom);

  engine.renderer().drawText(font, line1, 10, 10, 255, 255, 255, 255);
  engine.renderer().drawText(font, line2, 10, 32, 200, 200, 200, 255);
  engine.renderer().drawText(font, line3, 10, 54, 200, 200, 200, 255);
  engine.renderer().drawText(font, line4, 10, 76, 200, 200, 200, 255);
}

class DemoScene : public IScene
{
public:
  void onEnter(Engine &engine) override
  {
    auto &e = engine.scene().createEntity();
    e.transform.x = 100;
    e.transform.y = 100;

    font_ = engine.assets().loadFont("assets/Roboto-Regular.ttf", 20);

    e.sprite.texture = engine.assets().loadTexture("assets/player.png");
    e.sprite.enabled = true;
    e.sprite.scale = 1.0f;

    // desligar rect para nao desenhar quadrado
    e.rect.enabled = false;

    playerId_ = e.id;
  }

  void onUpdate(Engine &engine, float dt) override
  {
    auto &cam = engine.camera();
    Entity *p = engine.scene().findEntity(playerId_);
    if (!p)
      return;

    Input &input = engine.input();
    float x = input.getAxis("MoveX");
    float y = input.getAxis("MoveY");

    if (input.pressed("ToggleDebug"))
      engine.setScene(CreateDebugScene());

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

    float speed = 300.0f;
    p->transform.x += x * speed * dt;
    p->transform.y += y * speed * dt;

    cam.x = p->transform.x;
    cam.y = p->transform.y;
    if (cam.zoom < 0.1f)
      cam.zoom = 0.1f;
    if (cam.zoom > 6.0f)
      cam.zoom = 6.0f;
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
    auto &e = engine.scene().createEntity();
    e.transform.x = 100;
    e.transform.y = 100;

    font_ = engine.assets().loadFont("assets/Roboto-Regular.ttf", 20);

    e.rect.w = 40;
    e.rect.h = 40;
    e.rect.r = 80;
    e.rect.g = 200;
    e.rect.b = 255;
    e.rect.a = 255;
    e.rect.enabled = true;

    e.sprite.enabled = false;

    playerId_ = e.id;
  }

  void onUpdate(Engine &engine, float dt) override
  {
    auto &cam = engine.camera();
    Entity *p = engine.scene().findEntity(playerId_);
    if (!p)
      return;

    Input &input = engine.input();
    float x = input.getAxis("MoveX");
    float y = input.getAxis("MoveY");

    if (input.pressed("ToggleDebug"))
      engine.setScene(CreateDemoScene());

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

    float speed = 240.0f;
    p->transform.x += x * speed * dt;
    p->transform.y += y * speed * dt;

    cam.x = p->transform.x;
    cam.y = p->transform.y;
    if (cam.zoom < 0.1f)
      cam.zoom = 0.1f;
    if (cam.zoom > 6.0f)
      cam.zoom = 6.0f;
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
