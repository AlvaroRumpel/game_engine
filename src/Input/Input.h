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

class Input
{
public:
    void beginFrame(); // zera estados transitórios (se precisar no futuro)

    // Engine chama isso ao receber evento de teclado
    void setKeyDown(Key key, bool down);

    // API consumida pelo Game
    bool isKeyDown(Key key) const;

    // Axes (ex: "Horizontal", "Vertical")
    void setAxisMapping(const std::string &name, AxisMapping mapping);
    float getAxis(const std::string &name) const;

private:
    std::unordered_map<Key, bool> keys_;
    std::unordered_map<std::string, AxisMapping> axes_;
};
