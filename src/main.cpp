#include "Engine/Engine.h"
#include "Game/IGame.h"

class SandboxGame : public IGame
{
public:
  void onInit(Engine &engine) override
  {
    auto &e = engine.scene().createEntity();
    e.transform.x = 100;
    e.transform.y = 100;

    font_ = engine.assets().loadFont("assets/Roboto-Regular.ttf", 20);

    e.sprite.texture = engine.assets().loadTexture("assets/player.png");
    e.sprite.enabled = true;
    e.sprite.scale = 1.0f;

    // desligar rect para não desenhar quadrado
    e.rect.enabled = false;

    playerId_ = e.id;
  }

  void onUpdate(Engine &engine, float dt) override
  {
    auto &cam = engine.camera();
    float camSpeed = 400.0f * dt;
    Entity *p = engine.scene().findEntity(playerId_);
    if (!p)
      return;

    float x = engine.input().getAxis("Horizontal");
    float y = engine.input().getAxis("Vertical");

    if (engine.input().isKeyDown(Key::Up))
      cam.zoom *= 1.0f + 1.5f * dt;
    if (engine.input().isKeyDown(Key::Down))
      cam.zoom *= 1.0f - 1.5f * dt;

    float speed = 300.0f;
    p->transform.x += x * speed * dt;
    p->transform.y += y * speed * dt;
    if (cam.zoom < 0.1f)
      cam.zoom = 0.1f;
    if (cam.zoom > 6.0f)
      cam.zoom = 6.0f;
  }

  void onRender(Engine &engine) override
  {
    (void)engine;
    if (font_)
    {
      engine.renderer().drawText(*font_, "Engine OK - FPS/Stats depois", 10, 10, 255, 255, 255, 255);
    }
  }

private:
  int playerId_ = 0;
  std::shared_ptr<Texture> tex_;
  std::shared_ptr<Font> font_;
};

int main()
{
  Engine engine;
  SandboxGame game;
  engine.run(game);
  return 0;
}
