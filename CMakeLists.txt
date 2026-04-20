cmake_minimum_required(VERSION 3.16)
project(TacticalLite VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── Raspberry Pi 4 tuning (Optimized for ARM) ──────────────────────────────
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_compile_options(-O2)
  # Only apply these flags on Linux ARM (Pi), not Mac ARM
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64" AND NOT APPLE AND NOT WIN32)
      add_compile_options(-march=armv8-a+simd -mtune=cortex-a72)
  endif()
endif()

# ─── Setup Raylib (FetchContent handles the OS differences) ──────────────────
include(FetchContent)
FetchContent_Declare(
  raylib
  URL https://github.com/raysan5/raylib/archive/refs/tags/5.0.tar.gz
)
FetchContent_MakeAvailable(raylib)

# ─── Sources ──────────────────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Create executable
add_executable(TacticalLite ${SOURCES})

# ─── Headers ──────────────────────────────────────────────────────────────────
target_include_directories(TacticalLite PRIVATE src) # Simplified

# ─── Smart Linking (The Cross-Platform Way) ──────────────────────────────────
# Raylib target already knows most of its dependencies.
target_link_libraries(TacticalLite PRIVATE raylib)

# Only link math and pthread on Unix systems (Pi/Mac), not Windows
if(UNIX)
    target_link_libraries(TacticalLite PRIVATE m pthread)
endif()

# ─── Copy assets ──────────────────────────────────────────────────────────────
# Works on all platforms to ensure textures are where the .exe is
add_custom_command(TARGET TacticalLite POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:TacticalLite>/assets
)