#pragma once

#include <hyprland/src/layout/algorithm/TiledAlgorithm.hpp>

#include <typeinfo>

namespace Hyprmosaic {
[[nodiscard]] UP<Layout::ITiledAlgorithm> makeLayout();
[[nodiscard]] const std::type_info *layoutType() noexcept;
} // namespace Hyprmosaic
