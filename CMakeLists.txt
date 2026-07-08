cmake_minimum_required(VERSION 3.16)

project(vent C)
include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCES
    src/main.c
    src/config.c
    src/download.c
    src/ui.c
    src/threadpool.c
    src/paths.c
)

add_executable(vent ${SOURCES})

find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(ARCHIVE REQUIRED libarchive)
target_link_libraries(vent PRIVATE CURL::libcurl ${ARCHIVE_LIBRARIES})
target_include_directories(vent PRIVATE ${ARCHIVE_INCLUDE_DIRS})
target_compile_options(vent PRIVATE ${ARCHIVE_CFLAGS_OTHER})

target_include_directories(vent PRIVATE
    ${PROJECT_SOURCE_DIR}/include/
    ${PROJECT_SOURCE_DIR}/external/
)

target_compile_definitions(vent PRIVATE
    VENT_STD_INSTALL="${CMAKE_INSTALL_FULL_DATADIR}/vent/std"
    VENT_STD_SOURCE="${PROJECT_SOURCE_DIR}/std"
)
install(TARGETS vent
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/std/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/vent/std
)
