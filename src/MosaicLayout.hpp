#pragma once

#include <hyprland/src/layout/algorithm/TiledAlgorithm.hpp>

#include <typeinfo>

namespace Hyprmosaic {
UP<Layout::ITiledAlgorithm> makeLayout();
const std::type_info *layoutType();
} // namespace Hyprmosaic
