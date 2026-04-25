cmake_minimum_required(VERSION 3.20)
project(GhostReconFS_PathFix CXX)

# ── Force 32-bit ──────────────────────────────────────────────────────────────
set(CMAKE_GENERATOR_PLATFORM Win32 CACHE STRING "" FORCE)

# ── C++ Standard ─────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Fetch Microsoft Detours (header-only build) ───────────────────────────────
include(FetchContent)
FetchContent_Declare(
    detours
    GIT_REPOSITORY https://github.com/microsoft/Detours.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(detours)

# Build Detours as a static library
file(GLOB DETOURS_SRC "${detours_SOURCE_DIR}/src/*.cpp")
# Exclude the samples / test files that have their own main()
list(FILTER DETOURS_SRC EXCLUDE REGEX "(detoured|uimports)\\.cpp$")

add_library(detours_lib STATIC ${DETOURS_SRC})
target_include_directories(detours_lib PUBLIC "${detours_SOURCE_DIR}/src")
target_compile_definitions(detours_lib PRIVATE
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
)
# Detours must be compiled as 32-bit
set_target_properties(detours_lib PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

# ── ASI Plugin DLL ────────────────────────────────────────────────────────────
add_library(GhostReconFS_PathFix SHARED
    src/dllmain.cpp
)

# Rename output to .asi
set_target_properties(GhostReconFS_PathFix PROPERTIES
    OUTPUT_NAME "GhostReconFS-PathFix"
    SUFFIX      ".asi"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

target_include_directories(GhostReconFS_PathFix PRIVATE
    "${detours_SOURCE_DIR}/src"
)

target_link_libraries(GhostReconFS_PathFix PRIVATE
    detours_lib
    psapi
    advapi32   # Registry APIs
)

target_compile_definitions(GhostReconFS_PathFix PRIVATE
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
    UNICODE
    _UNICODE
)

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS GhostReconFS_PathFix
    RUNTIME DESTINATION .
    LIBRARY DESTINATION .
)
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/GhostReconFS-PathFix.ini"
    DESTINATION .
)
