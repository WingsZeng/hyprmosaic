cmake_minimum_required(VERSION 3.19)
project(hyprmosaic LANGUAGES CXX)

find_package(PkgConfig REQUIRED)
pkg_check_modules(HYPRLAND REQUIRED IMPORTED_TARGET hyprland)

add_library(hyprmosaic SHARED src/main.cpp src/Config.cpp src/MosaicLayout.cpp)
target_compile_features(hyprmosaic PRIVATE cxx_std_23)
target_include_directories(hyprmosaic PRIVATE Hyprdrive/src/layout)
target_link_libraries(hyprmosaic PRIVATE PkgConfig::HYPRLAND)
set_target_properties(hyprmosaic PROPERTIES PREFIX "")
