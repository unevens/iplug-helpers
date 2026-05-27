# iplug-helpers — MacOSLinkageGate.cmake
#
# Fails the build if a built macOS plugin bundle would depend on a dynamic
# library that end-user machines do not have. The motivating case: Homebrew
# LLVM's libc++ / libunwind under /opt/homebrew. We build with Homebrew
# Clang >= 22 (see HomebrewClang22Preflight.cmake), and while that toolchain
# *currently* links the SDK's /usr/lib/libc++.1.dylib, that behaviour is
# toolchain-version- and flag-dependent. A bundle that picks up a
# /opt/homebrew dylib (directly, or via an @rpath that resolves there)
# silently fails to load on every machine but the build host. This gate
# turns that latent shipping bug into a hard build failure.
#
# Dual-mode file:
#   * include()d from a CMakeLists  -> defines
#       iplug_helpers_add_macos_linkage_gate(<target>...)
#     which attaches a POST_BUILD step that runs this same file in script
#     mode against each target's built Mach-O binary.
#   * run via `cmake -DGATE_BINARY=<path> -P MacOSLinkageGate.cmake`
#     -> scans that binary and FATAL_ERRORs on any stray dependency.
#
# What counts as "system" (allowed):
#   * dylib deps under /usr/lib/ or /System/Library/
#   * dylib deps via @rpath/ , @loader_path/ , @executable_path/
#   * LC_RPATH entries that are bundle-relative (@loader_path/@executable_path)
#     or themselves under /usr/lib/ or /System/Library/
# Anything else — most importantly absolute paths or rpaths under
# /opt/homebrew, /usr/local, /opt/local — is rejected. Checking the rpaths
# (not just the dep paths) is what catches the "@rpath/libc++.1.dylib + an
# LC_RPATH baked into /opt/homebrew" form of the leak.

# ---------------------------------------------------------------------------
# Script mode: scan a single binary.
# ---------------------------------------------------------------------------
if(CMAKE_SCRIPT_MODE_FILE AND CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
  if(NOT DEFINED GATE_BINARY)
    message(FATAL_ERROR "MacOSLinkageGate: -DGATE_BINARY=<path> is required in script mode")
  endif()
  if(NOT EXISTS "${GATE_BINARY}")
    message(FATAL_ERROR "MacOSLinkageGate: binary not found: ${GATE_BINARY}")
  endif()

  find_program(_otool otool)
  if(NOT _otool)
    message(FATAL_ERROR "MacOSLinkageGate: otool not found on PATH")
  endif()

  set(_bad "")

  # --- linked dylibs (otool -L) -------------------------------------------
  execute_process(
    COMMAND "${_otool}" -L "${GATE_BINARY}"
    OUTPUT_VARIABLE _deps RESULT_VARIABLE _rc ERROR_VARIABLE _err)
  if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "MacOSLinkageGate: 'otool -L' failed for ${GATE_BINARY}:\n${_err}")
  endif()
  string(REPLACE "\n" ";" _lines "${_deps}")
  foreach(_line IN LISTS _lines)
    # dependency lines are tab-indented; the "<file>:" header line is not
    if(NOT _line MATCHES "^\t")
      continue()
    endif()
    string(STRIP "${_line}" _line)
    # drop the trailing " (compatibility version ..., current version ...)"
    string(REGEX REPLACE " \\(compatibility version.*$" "" _path "${_line}")
    if(_path MATCHES "^/usr/lib/"
       OR _path MATCHES "^/System/Library/"
       OR _path MATCHES "^@rpath/"
       OR _path MATCHES "^@loader_path/"
       OR _path MATCHES "^@executable_path/")
      continue()
    endif()
    list(APPEND _bad "dep:   ${_path}")
  endforeach()

  # --- runtime search paths (LC_RPATH via otool -l) -----------------------
  execute_process(
    COMMAND "${_otool}" -l "${GATE_BINARY}"
    OUTPUT_VARIABLE _loadcmds RESULT_VARIABLE _rc2 ERROR_VARIABLE _err2)
  if(NOT _rc2 EQUAL 0)
    message(FATAL_ERROR "MacOSLinkageGate: 'otool -l' failed for ${GATE_BINARY}:\n${_err2}")
  endif()
  string(REPLACE "\n" ";" _llines "${_loadcmds}")
  set(_in_rpath FALSE)
  foreach(_l IN LISTS _llines)
    string(STRIP "${_l}" _l)
    if(_l STREQUAL "cmd LC_RPATH")
      set(_in_rpath TRUE)
    elseif(_in_rpath AND _l MATCHES "^path (.+) \\(offset")
      set(_rp "${CMAKE_MATCH_1}")
      set(_in_rpath FALSE)
      if(_rp MATCHES "^@loader_path"
         OR _rp MATCHES "^@executable_path"
         OR _rp MATCHES "^@rpath"
         OR _rp MATCHES "^/usr/lib/"
         OR _rp MATCHES "^/System/Library/")
        continue()
      endif()
      list(APPEND _bad "rpath: ${_rp}")
    endif()
  endforeach()

  if(_bad)
    string(REPLACE ";" "\n    " _bad_pretty "${_bad}")
    message(FATAL_ERROR
      "MacOSLinkageGate: ${GATE_BINARY}\n"
      "references non-system libraries / search paths that end users will not have:\n"
      "    ${_bad_pretty}\n"
      "Allowed: deps under /usr/lib/ or /System/Library/ or via "
      "@rpath//@loader_path//@executable_path; rpaths that are bundle-relative "
      "or system.\n"
      "This usually means the toolchain (e.g. Homebrew LLVM libc++/libunwind) "
      "leaked into the link. Fix the build so the bundle depends only on the "
      "macOS system libraries before shipping it.")
  endif()
  message(STATUS "MacOSLinkageGate: OK — ${GATE_BINARY} references only system libraries")
  return()
endif()

# ---------------------------------------------------------------------------
# Include mode: define the helper. Capture this file's path now (at include
# time CMAKE_CURRENT_LIST_FILE is this module; inside the function it would
# be the caller's list file).
# ---------------------------------------------------------------------------
set(_IPLUG_HELPERS_LINKAGE_GATE_SCRIPT "${CMAKE_CURRENT_LIST_FILE}"
    CACHE INTERNAL "Path to MacOSLinkageGate.cmake for script-mode invocation")

function(iplug_helpers_add_macos_linkage_gate)
  if(NOT APPLE)
    return()
  endif()
  foreach(_tgt IN LISTS ARGV)
    if(NOT TARGET ${_tgt})
      continue()
    endif()
    add_custom_command(TARGET ${_tgt} POST_BUILD
      COMMAND "${CMAKE_COMMAND}"
              "-DGATE_BINARY=$<TARGET_FILE:${_tgt}>"
              -P "${_IPLUG_HELPERS_LINKAGE_GATE_SCRIPT}"
      VERBATIM
      COMMENT "Linkage gate: checking ${_tgt} references only macOS system libraries")
  endforeach()
endfunction()
