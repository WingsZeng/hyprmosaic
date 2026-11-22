#include "Config.hpp"

#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/config/values/types/FloatValue.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>

#include <algorithm>
#include <stdexcept>

namespace {
constexpr auto DEFAULT_WIDTH_KEY = "plugin:hyprmosaic:default_width";
constexpr auto DEFAULT_HEIGHT_KEY = "plugin:hyprmosaic:default_height";
constexpr auto FOCUS_MODE_KEY = "plugin:hyprmosaic:focus_mode";

constexpr auto DEFAULT_SIZE = 0.6F;
constexpr auto DEFAULT_FOCUS_MODE = 1;

CConfigValue<Hyprlang::FLOAT> defaultWidth;
CConfigValue<Hyprlang::FLOAT> defaultHeight;
CConfigValue<Hyprlang::INT> focusMode;

double resolveSize(double value, double extent) {
  return std::max(1.0, value <= 1.0 ? value * extent : value);
}
} // namespace

void Hyprmosaic::Config::registerValues(HANDLE handle) {
  using namespace ::Config::Values;

  auto width = makeShared<CFloatValue>(
      DEFAULT_WIDTH_KEY,
      "default window width; <= 1 is work-area ratio, > 1 is pixels",
      DEFAULT_SIZE, SFloatValueOptions{.min = 0.001F});
  auto height = makeShared<CFloatValue>(
      DEFAULT_HEIGHT_KEY,
      "default window height; <= 1 is work-area ratio, > 1 is pixels",
      DEFAULT_SIZE, SFloatValueOptions{.min = 0.001F});

  auto focus =
      makeShared<CIntValue>(FOCUS_MODE_KEY, "focus mode", DEFAULT_FOCUS_MODE,
                            SIntValueOptions{.min = 0, .max = 1});

  if (!HyprlandAPI::addConfigValueV2(handle, width) ||
      !HyprlandAPI::addConfigValueV2(handle, height) ||
      !HyprlandAPI::addConfigValueV2(handle, focus))
    throw std::runtime_error("failed to register hyprmosaic config values");

  defaultWidth.bind(DEFAULT_WIDTH_KEY);
  defaultHeight.bind(DEFAULT_HEIGHT_KEY);
  focusMode.bind(FOCUS_MODE_KEY);
}

Vector2D Hyprmosaic::Config::defaultSize(const Vector2D &workAreaSize) {
  return {resolveSize(*defaultWidth, workAreaSize.x),
          resolveSize(*defaultHeight, workAreaSize.y)};
}

bool Hyprmosaic::Config::focusModeEnabled() { return *focusMode != 0; }
