cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0135 NEW)

project(vent C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Windows: fetch deps via CMake; Linux/macOS: use system packages
if(WIN32)
    option(VENT_FETCH_DEPS "Download and build dependencies via CMake" ON)
else()
    option(VENT_FETCH_DEPS "Download and build dependencies via CMake" OFF)
endif()

if(VENT_FETCH_DEPS)
    include(FetchContent)

    set(CURL_VERSION 8.21.0)
    set(ZLIB_VERSION 1.3.1)
    set(LIBARCHIVE_VERSION 3.8.8)

    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(HTTP_ONLY ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_LDAP ON CACHE BOOL "" FORCE)
    if(MSVC)
        set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
    endif()
    FetchContent_Declare(curl
        URL https://github.com/curl/curl/releases/download/curl-${CURL_VERSION}/curl-${CURL_VERSION}.tar.gz
    )
    FetchContent_MakeAvailable(curl)

    FetchContent_Declare(zlib
        URL https://github.com/madler/zlib/releases/download/v${ZLIB_VERSION}/zlib-${ZLIB_VERSION}.tar.gz
    )
    FetchContent_MakeAvailable(zlib)
    FetchContent_GetProperties(zlib)
    set(ZLIB_FOUND TRUE CACHE BOOL "" FORCE)
    set(ZLIB_INCLUDE_DIR "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}" CACHE STRING "" FORCE)
    if(TARGET zlibstatic)
        set(ZLIB_LIBRARY zlibstatic CACHE STRING "" FORCE)
    elseif(TARGET zlib)
        set(ZLIB_LIBRARY zlib CACHE STRING "" FORCE)
    endif()

    set(ENABLE_TEST OFF CACHE BOOL "" FORCE)
    set(ENABLE_TAR OFF CACHE BOOL "" FORCE)
    set(ENABLE_CPIO OFF CACHE BOOL "" FORCE)
    set(ENABLE_CAT OFF CACHE BOOL "" FORCE)
    set(ENABLE_UNZIP OFF CACHE BOOL "" FORCE)
    set(ENABLE_RAR OFF CACHE BOOL "" FORCE)
    set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(libarchive
        URL https://github.com/libarchive/libarchive/releases/download/v${LIBARCHIVE_VERSION}/libarchive-${LIBARCHIVE_VERSION}.tar.gz
    )
    FetchContent_MakeAvailable(libarchive)
else()
    find_package(CURL REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ARCHIVE REQUIRED libarchive)
endif()

set(SOURCES
    src/main.c
    src/config.c
    src/download.c
    src/ui.c
)

add_executable(vent ${SOURCES})

if(VENT_FETCH_DEPS)
    target_link_libraries(vent PRIVATE CURL::libcurl archive)
else()
    target_link_libraries(vent PRIVATE CURL::libcurl ${ARCHIVE_LIBRARIES})
    target_include_directories(vent PRIVATE ${ARCHIVE_INCLUDE_DIRS})
    target_compile_options(vent PRIVATE ${ARCHIVE_CFLAGS_OTHER})
endif()

target_include_directories(vent PRIVATE
    ${PROJECT_SOURCE_DIR}/include/
    ${PROJECT_SOURCE_DIR}/external/
)
target_compile_definitions(vent PUBLIC
    VENT_STD="${PROJECT_SOURCE_DIR}/std"
)
