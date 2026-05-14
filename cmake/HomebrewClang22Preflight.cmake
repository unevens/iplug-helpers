# iplug-helpers — HomebrewClang22Preflight.cmake
#
# macOS arm64 toolchain preflight.
#
# Apple Clang (Xcode 17, CLT 21) and pre-22 upstream Clang have a NEON
# codegen bug that triggers a frontend bus error in the heavy SIMD spline
# DSP TUs (CurvessorDsp.cpp, OverdrawDsp.cpp). The fix shipped in upstream
# LLVM 22, so we require Homebrew Clang >= 22 here. Verified working:
# Homebrew clang 22.1.4.
#
# Usage from a plugin's top-level CMakeLists:
#   include(${CMAKE_CURRENT_LIST_DIR}/../iplug-helpers/cmake/HomebrewClang22Preflight.cmake)
#   iplug_helpers_require_homebrew_clang_22("MyPlugin" "Source/MyPluginDsp.cpp")
#
# `${plugin_name}` and `${dsp_tu_hint}` go into the error message so users
# know which plugin and which TU is forcing the requirement.

function(iplug_helpers_require_homebrew_clang_22 plugin_name dsp_tu_hint)
  if(NOT APPLE)
    return()
  endif()
  if(NOT (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64"
          OR CMAKE_OSX_ARCHITECTURES MATCHES "arm64"))
    return()
  endif()

  if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"
          OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
              AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 22)))
    return()
  endif()

  # Be helpful: if Homebrew llvm is already installed, point the user at
  # it; otherwise tell them how to install it.
  set(_brew_clang "")
  foreach(_p /opt/homebrew/opt/llvm/bin/clang++
             /usr/local/opt/llvm/bin/clang++)
    if(EXISTS "${_p}")
      set(_brew_clang "${_p}")
      break()
    endif()
  endforeach()

  if(_brew_clang)
    string(REPLACE "++" "" _brew_cc "${_brew_clang}")
    message(FATAL_ERROR
        "${plugin_name} on macOS arm64 requires Homebrew Clang >= 22 "
        "(detected: ${CMAKE_CXX_COMPILER_ID} "
        "${CMAKE_CXX_COMPILER_VERSION}). Apple Clang and pre-22 "
        "upstream Clang hit a NEON codegen bus error in "
        "${dsp_tu_hint}. You already have a suitable compiler "
        "installed; reconfigure with:\n"
        "  rm -rf ${CMAKE_BINARY_DIR}\n"
        "  cmake -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR} \\\n"
        "    -DCMAKE_C_COMPILER=${_brew_cc} \\\n"
        "    -DCMAKE_CXX_COMPILER=${_brew_clang}")
  else()
    message(FATAL_ERROR
        "${plugin_name} on macOS arm64 requires Homebrew Clang >= 22 "
        "(detected: ${CMAKE_CXX_COMPILER_ID} "
        "${CMAKE_CXX_COMPILER_VERSION}). Install with:\n"
        "  brew install llvm\n"
        "and reconfigure with:\n"
        "  rm -rf ${CMAKE_BINARY_DIR}\n"
        "  cmake -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR} \\\n"
        "    -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \\\n"
        "    -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++")
  endif()
endfunction()
