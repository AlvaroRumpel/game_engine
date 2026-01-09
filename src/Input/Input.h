#pragma once
#include <string>
#include <unordered_map>
#include <vector>

enum class Key
{
    Unknown = 0,
    A,
    D,
    W,
    S,
    Q,
    E,
    Tab,
    Left,
    Right,
    Up,
    Down,
    Escape
};

struct AxisMapping
{
    std::vector<Key> positive;
    std::vector<Key> negative;
};

struct ActionBinding
{
    std::vector<Key> keys;
    int wheelDir = 0; // +1 para wheel up, -1 para wheel down, 0 = none
};

class Input
{
public:
    void beginFrame(); // zera estados transitórios (se precisar no futuro)

    // Engine chama isso ao receber evento de teclado
    void setKeyDown(Key key, bool down);

    // API consumida pelo Game
    bool isKeyDown(Key key) const;

    // Axes (ex: "MoveX", "MoveY")
    void setAxisMapping(const std::string &name, AxisMapping mapping);
    float getAxis(const std::string &name) const;

    // Actions (ex: "ToggleDebug")
    void setActionBinding(const std::string &name, ActionBinding binding);
    bool down(const std::string &name) const;
    bool pressed(const std::string &name) const;

    // Mouse (screen-space)
    void setMousePosition(int x, int y);
    void setMouseWheelDelta(float delta);
    int mouseX() const { return mouseX_; }
    int mouseY() const { return mouseY_; }

private:
    std::unordered_map<Key, bool> keys_;
    std::unordered_map<Key, bool> prevKeys_;
    std::unordered_map<std::string, AxisMapping> axes_;
    std::unordered_map<std::string, ActionBinding> actions_;

    int mouseX_ = 0;
    int mouseY_ = 0;
    float mouseWheelDelta_ = 0.0f;
};
