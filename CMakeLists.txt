cmake_minimum_required(VERSION 3.16)
project(MTSLib VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Library ---
add_library(mtslib
    src/task.cpp
)

target_include_directories(mtslib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(Threads REQUIRED)
target_link_libraries(mtslib PUBLIC Threads::Threads)

# --- Examples ---
add_subdirectory(examples)
