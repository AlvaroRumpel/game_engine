#include "Input.h"

void Input::beginFrame()
{
    // Hoje não temos "pressed/released" (só down).
    // No futuro: limpar estados de transição aqui.
}

void Input::setKeyDown(Key key, bool down)
{
    keys_[key] = down;
}

bool Input::isKeyDown(Key key) const
{
    auto it = keys_.find(key);
    if (it == keys_.end())
        return false;
    return it->second;
}

void Input::setAxisMapping(const std::string &name, AxisMapping mapping)
{
    axes_[name] = std::move(mapping);
}

float Input::getAxis(const std::string &name) const
{
    auto it = axes_.find(name);
    if (it == axes_.end())
        return 0.0f;

    float v = 0.0f;

    for (auto k : it->second.positive)
    {
        if (isKeyDown(k))
            v += 1.0f;
    }
    for (auto k : it->second.negative)
    {
        if (isKeyDown(k))
            v -= 1.0f;
    }

    // clamp simples
    if (v > 1.0f)
        v = 1.0f;
    if (v < -1.0f)
        v = -1.0f;

    return v;
}
