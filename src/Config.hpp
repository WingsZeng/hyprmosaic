#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprutils/math/Vector2D.hpp>

#include <cstdint>

namespace Hyprmosaic::Config {
enum class FocusMode : std::uint8_t {
  Smart,
  Center,
  Count,
};

void registerValues(HANDLE handle);
[[nodiscard]] Vector2D defaultSize(const Vector2D &workAreaSize);
[[nodiscard]] FocusMode focusMode();
} // namespace Hyprmosaic::Config
