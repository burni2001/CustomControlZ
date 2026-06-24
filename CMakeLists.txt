cmake_minimum_required(VERSION 3.20)
project(CustomControlZ VERSION 1.0 LANGUAGES CXX OBJCXX)

# ──────────────────────────────────────────────────────────────────────────────
# This CMakeLists.txt is macOS-only.
# The Windows build continues to use src/CustomControlZ.vcxproj / build.bat.
# ──────────────────────────────────────────────────────────────────────────────
if(NOT APPLE)
    message(FATAL_ERROR
        "This CMakeLists.txt is for macOS only.\n"
        "For Windows, use build.bat (MSBuild / Visual Studio).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)

# ──────────────────────────────────────────────────────────────────────────────
# Qt 6
# Homebrew path: /opt/homebrew/opt/qt@6 (Apple Silicon)
#                /usr/local/opt/qt@6     (Intel)
# ──────────────────────────────────────────────────────────────────────────────
if(NOT DEFINED CMAKE_PREFIX_PATH)
    # Auto-detect Homebrew Qt location
    execute_process(
        COMMAND brew --prefix qt@6
        OUTPUT_VARIABLE QT_HOMEBREW_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(QT_HOMEBREW_PREFIX)
        list(APPEND CMAKE_PREFIX_PATH "${QT_HOMEBREW_PREFIX}")
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# Enable Qt automatic tools
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ──────────────────────────────────────────────────────────────────────────────
# macOS frameworks
# ──────────────────────────────────────────────────────────────────────────────
find_library(APPKIT_LIB               AppKit               REQUIRED)
find_library(COREGRAPHICS_LIB         CoreGraphics         REQUIRED)
find_library(APPLICATIONSERVICES_LIB  ApplicationServices  REQUIRED)
find_library(COREFOUNDATION_LIB       CoreFoundation       REQUIRED)

# ──────────────────────────────────────────────────────────────────────────────
# Sources
# ──────────────────────────────────────────────────────────────────────────────
set(SOURCES
    src/platform/platform_macos.mm
    src/mac/GameEngine.cpp
    src/mac/TrayIcon.cpp
    src/mac/SettingsWindow.cpp
    src/mac/main_mac.cpp
)

# ──────────────────────────────────────────────────────────────────────────────
# App bundle target
# ──────────────────────────────────────────────────────────────────────────────
add_executable(CustomControlZ MACOSX_BUNDLE ${SOURCES})

set_target_properties(CustomControlZ PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/src/mac/Info.plist"
    MACOSX_BUNDLE_BUNDLE_NAME "CustomControlZ"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.customcontrolz.app"
)

# ──────────────────────────────────────────────────────────────────────────────
# Include paths
#
# CRITICAL: src/platform must come BEFORE all others so that
#   #include <windows.h> inside BehaviorEngine.h finds our shim instead
#   of looking in system paths (where it doesn't exist on macOS).
# ──────────────────────────────────────────────────────────────────────────────
target_include_directories(CustomControlZ BEFORE PRIVATE
    "${CMAKE_SOURCE_DIR}/src/platform"  # windows.h shim + win_types_compat.h
    "${CMAKE_SOURCE_DIR}/src"           # GameProfiles.h, BehaviorEngine.h, games/
    "${CMAKE_SOURCE_DIR}/src/mac"       # IniConfig.h, GameEngine.h, …
)

# ──────────────────────────────────────────────────────────────────────────────
# Link libraries
# ──────────────────────────────────────────────────────────────────────────────
target_link_libraries(CustomControlZ PRIVATE
    Qt6::Core
    Qt6::Widgets
    ${APPKIT_LIB}
    ${COREGRAPHICS_LIB}
    ${APPLICATIONSERVICES_LIB}
    ${COREFOUNDATION_LIB}
)

# ──────────────────────────────────────────────────────────────────────────────
# Compiler flags
# ──────────────────────────────────────────────────────────────────────────────
target_compile_options(CustomControlZ PRIVATE
    -Wall -Wextra
    -Wno-unused-parameter        # game profiles declare many unused ones
    -Wno-missing-field-initializers
    -Wno-switch                  # GameProfiles.h switch has intentional fall-throughs
)

# Suppress the "designated initializers" warning from game profile headers
# (they use C99-style designated initialisers in C++ which clang allows)
set_source_files_properties(src/mac/GameEngine.cpp PROPERTIES
    COMPILE_FLAGS "-Wno-c99-designator")

# ──────────────────────────────────────────────────────────────────────────────
# Post-build: macdeployqt (copies Qt frameworks into the .app bundle)
# ──────────────────────────────────────────────────────────────────────────────
find_program(MACDEPLOYQT_EXECUTABLE macdeployqt
    HINTS "${QT_HOMEBREW_PREFIX}/bin" "/opt/homebrew/bin" "/usr/local/bin")

if(MACDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET CustomControlZ POST_BUILD
        COMMAND "${MACDEPLOYQT_EXECUTABLE}"
                "$<TARGET_BUNDLE_DIR:CustomControlZ>"
                -always-overwrite
        COMMENT "Running macdeployqt…"
        VERBATIM)
else()
    message(WARNING "macdeployqt not found — run it manually after building")
endif()

# ──────────────────────────────────────────────────────────────────────────────
# Install target (optional; used by package_mac.sh)
# ──────────────────────────────────────────────────────────────────────────────
install(TARGETS CustomControlZ BUNDLE DESTINATION .)
