cmake_minimum_required(VERSION 3.16)
project(TacticalLite VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── Build type ───────────────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ─── Raspberry Pi 4 tuning ────────────────────────────────────────────────────
# Targets the Cortex-A72 (BCM2711) with NEON SIMD; strip dead code.
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_compile_options(-O2)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64|ARM64")
    if(APPLE)
      add_compile_options(-mcpu=apple-m1)   # or omit entirely, clang auto-tunes
    else()
      add_compile_options(-march=armv8-a+simd -mtune=cortex-a72)
    endif()
  endif()
endif()

# ─── Find Raylib ──────────────────────────────────────────────────────────────
# Install raylib via: sudo apt install libraylib-dev  OR build from source.
find_package(raylib 4.5 REQUIRED)

# ─── Sources ──────────────────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES
  src/*.cpp
  src/ai/*.cpp
  src/weapons/*.cpp
  src/utility/*.cpp
  src/audio/*.cpp
  src/render/*.cpp
  src/game/*.cpp
)

add_executable(TacticalLite ${SOURCES}
        src/main.cpp
        src/main.cpp
        src/audio/AudioSystem.h)

target_include_directories(TacticalLite PRIVATE
  src
  src/ai
  src/weapons
  src/utility
  src/audio
  src/render
  src/game
)

target_link_libraries(TacticalLite PRIVATE raylib m pthread)

# ─── Copy assets next to binary ───────────────────────────────────────────────
add_custom_command(TARGET TacticalLite POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:TacticalLite>/assets
)
