# iplug-helpers

Shared header-only helpers for `unevens/Curvessor` and `unevens/Overdraw`
ports to [iPlug2](https://github.com/iPlug2/iPlug2). Common visual style,
custom controls, layout patterns, and CMake preflight checks pulled out
of each plugin's `iplug/Plugin.cpp` so the two ports stay in sync.

## Layout

```
cmake/
  HomebrewClang22Preflight.cmake     macOS arm64 toolchain check
include/iplug-helpers/
  controls/
    Palette.hpp                      IColor + IText constants
    LightMarkerMeterControl.hpp      IVMeterControl with readable dB labels
  layout/                            (TBD — geometry helpers)
  util/                              (TBD — linkable-pair propagation, etc.)
```

## Usage from a plugin's CMakeLists

```cmake
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../iplug-helpers iplug-helpers)
target_link_libraries(${PROJECT_NAME}-vst3 PRIVATE iplug-helpers)
```

In the plugin's `.cpp`:

```cpp
#include "iplug-helpers/controls/Palette.hpp"
#include "iplug-helpers/controls/LightMarkerMeterControl.hpp"
```

## License

Same as the consuming plugins (GPL-3.0). Copyright Dario Mambro 2020–2026.
