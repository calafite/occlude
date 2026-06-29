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
FetchContent_MakeAvailable(json)

add_library(occlude_lib STATIC
    lib/fs.cpp
    lib/hash.cpp
    lib/state.cpp
)

target_include_directories(occlude_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

target_link_libraries(occlude_lib PUBLIC
    nlohmann_json::nlohmann_json
)

add_executable(occluded
    daemon/main.cpp
    daemon/scanner.cpp
    daemon/dispatcher.cpp
)

target_include_directories(occluded PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/daemon
)

target_link_libraries(occluded PRIVATE
    occlude_lib
)

add_executable(occlude
    interface/main.cpp
)

target_include_directories(occlude PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/interface
)

target_link_libraries(occlude PRIVATE
    occlude_lib
)
