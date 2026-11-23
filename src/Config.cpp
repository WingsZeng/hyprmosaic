#include "Config.hpp"

#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/config/values/types/FloatValue.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace {
constexpr auto DEFAULT_WIDTH_KEY = "plugin:hyprmosaic:default_width";
constexpr auto DEFAULT_HEIGHT_KEY = "plugin:hyprmosaic:default_height";
constexpr auto FOCUS_MODE_KEY = "plugin:hyprmosaic:focus_mode";

constexpr auto DEFAULT_SIZE = 0.6F;
constexpr auto DEFAULT_FOCUS_MODE =
    std::to_underlying(Hyprmosaic::Config::FocusMode::Smart);

CConfigValue<Hyprlang::FLOAT> defaultWidthValue, defaultHeightValue;
CConfigValue<Hyprlang::INT> focusModeValue;

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

  auto focus = makeShared<CIntValue>(
      FOCUS_MODE_KEY, "focus mode: 0 = smart, 1 = center", DEFAULT_FOCUS_MODE,
      SIntValueOptions{
          .min = 0,
          .max = std::to_underlying(Hyprmosaic::Config::FocusMode::Count) - 1});

  if (!HyprlandAPI::addConfigValueV2(handle, std::move(width)) ||
      !HyprlandAPI::addConfigValueV2(handle, std::move(height)) ||
      !HyprlandAPI::addConfigValueV2(handle, std::move(focus)))
    throw std::runtime_error("failed to register hyprmosaic config values");

  defaultWidthValue.bind(DEFAULT_WIDTH_KEY);
  defaultHeightValue.bind(DEFAULT_HEIGHT_KEY);
  focusModeValue.bind(FOCUS_MODE_KEY);
}

Vector2D Hyprmosaic::Config::defaultSize(const Vector2D &workAreaSize) {
  return {resolveSize(*defaultWidthValue, workAreaSize.x),
          resolveSize(*defaultHeightValue, workAreaSize.y)};
}

Hyprmosaic::Config::FocusMode Hyprmosaic::Config::focusMode() {
  return static_cast<FocusMode>(*focusModeValue);
}
