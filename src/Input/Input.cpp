#include "Input.h"

void Input::beginFrame()
{
    prevKeys_ = keys_;
    mouseWheelDelta_ = 0.0f;
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

void Input::setActionBinding(const std::string &name, ActionBinding binding)
{
    actions_[name] = std::move(binding);
}

bool Input::down(const std::string &name) const
{
    auto it = actions_.find(name);
    if (it == actions_.end())
        return false;

    for (auto k : it->second.keys)
    {
        if (isKeyDown(k))
            return true;
    }

    return false;
}

bool Input::pressed(const std::string &name) const
{
    auto it = actions_.find(name);
    if (it == actions_.end())
        return false;

    for (auto k : it->second.keys)
    {
        bool now = isKeyDown(k);
        bool before = false;
        auto pit = prevKeys_.find(k);
        if (pit != prevKeys_.end())
            before = pit->second;
        if (now && !before)
            return true;
    }

    if (it->second.wheelDir != 0)
    {
        if (mouseWheelDelta_ * (float)it->second.wheelDir > 0.0f)
            return true;
    }

    return false;
}

void Input::setMousePosition(int x, int y)
{
    mouseX_ = x;
    mouseY_ = y;
}

void Input::setMouseWheelDelta(float delta)
{
    mouseWheelDelta_ += delta;
}
