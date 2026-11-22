#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprutils/math/Vector2D.hpp>

namespace Hyprmosaic::Config {
void registerValues(HANDLE handle);
Vector2D defaultSize(const Vector2D &workAreaSize);
bool focusModeEnabled();
} // namespace Hyprmosaic::Config
