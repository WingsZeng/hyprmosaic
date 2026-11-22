#include "Config.hpp"
#include "MosaicLayout.hpp"

#include <stdexcept>
#include <string>

APICALL EXPORT std::string PLUGIN_API_VERSION() { return HYPRLAND_API_VERSION; }

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
  if (std::string{HyprlandAPI::getHyprlandVersion(handle).hash} !=
      GIT_COMMIT_HASH)
    throw std::runtime_error(
        "hyprmosaic was built against a different Hyprland version");

  Hyprmosaic::Config::registerValues(handle);

  if (!HyprlandAPI::addTiledAlgo(handle, "mosaic", Hyprmosaic::layoutType(),
                                 [] { return Hyprmosaic::makeLayout(); }))
    throw std::runtime_error("failed to register the mosaic layout");
  return {"hyprmosaic", "The Mosaic layout separated from Hyprdrive",
          "WingsZeng", "0.1.0"};
}
