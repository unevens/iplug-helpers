cmake_minimum_required(VERSION 3.25)
project(iplug-helpers VERSION 0.1.0 LANGUAGES CXX)

# Header-only INTERFACE library — consumers add it to target_link_libraries
# and pick up the include path. No compiled artefacts.
add_library(iplug-helpers INTERFACE)
add_library(iplug-helpers::iplug-helpers ALIAS iplug-helpers)

target_include_directories(iplug-helpers INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include)

target_compile_features(iplug-helpers INTERFACE cxx_std_17)
