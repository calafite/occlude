cmake_minimum_required(VERSION 3.22)
project(occlude LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-O2 -Wall -Wextra -Wpedantic)

include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
    GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(json ftxui)

# Combined library containing all core domain, IO, and utilities
add_library(occlude_lib STATIC
    lib/io/fs.cpp
    lib/utils/hash.cpp
    lib/core/state.cpp
)

target_include_directories(occlude_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/3p
)

target_link_libraries(occlude_lib PUBLIC
    nlohmann_json::nlohmann_json
)

# Daemon executable
add_executable(occluded
    apps/daemon/main.cpp
    apps/daemon/scanner.cpp
    apps/daemon/dispatcher.cpp
)

target_include_directories(occluded PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/daemon
)

target_link_libraries(occluded PRIVATE
    occlude_lib
)

# Command-Line Client
add_executable(occlude
    apps/cli/main.cpp
)

target_include_directories(occlude PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/cli
)

target_link_libraries(occlude PRIVATE
    occlude_lib
)

add_executable(occlude-tui
    apps/tui/main.cpp
    apps/tui/ipcClient.cpp
    apps/tui/components.cpp
    apps/tui/modals.cpp
)

target_include_directories(occlude-tui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/tui
)

target_link_libraries(occlude-tui PRIVATE
    occlude_lib
    ftxui::screen
    ftxui::dom
    ftxui::component
)
