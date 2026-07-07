cmake_minimum_required(VERSION 3.16)

project(vent C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCES
    src/main.c
    src/config.c
    src/download.c
    src/ui.c
)

add_executable(vent ${SOURCES})

find_package(CURL REQUIRED)
target_link_libraries(vent PRIVATE CURL::libcurl)

target_include_directories(vent PRIVATE
    ${PROJECT_SOURCE_DIR}/include/
    ${PROJECT_SOURCE_DIR}/external/
)
target_compile_definitions(vent PUBLIC
    VENT_STD="${PROJECT_SOURCE_DIR}/std"
)
