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
    src/sha256.c
    src/option.c
)

if(WIN32)
  set(CMAKE_C_STANDARD 17)
  add_definitions(-D_WIN32_WINNT=0x0A00 -D_WINSOCK_DEPRECATED_NO_WARNINGS)
  set(SOURCES ${SOURCES} src/platform_win.c)
else()
  set(SOURCES ${SOURCES} src/platform_posix.c)
endif()

add_executable(vent ${SOURCES})

if(WIN32)
  find_package(PkgConfig QUIET)

  if(PkgConfig_FOUND)
    pkg_check_modules(CURL QUIET libcurl)
    pkg_check_modules(ARCHIVE QUIET libarchive)
  endif()

  if(NOT CURL_FOUND)
    find_path(CURL_INCLUDE_DIR curl/curl.h)
    find_library(CURL_LIBRARY libcurl)
    if(CURL_INCLUDE_DIR AND CURL_LIBRARY)
      set(CURL_FOUND TRUE)
      set(CURL_INCLUDE_DIRS ${CURL_INCLUDE_DIR})
      set(CURL_LIBRARIES ${CURL_LIBRARY})
    endif()
  endif()

  if(NOT ARCHIVE_FOUND)
    find_path(ARCHIVE_INCLUDE_DIR archive.h)
    find_library(ARCHIVE_LIBRARY archive)
    if(ARCHIVE_INCLUDE_DIR AND ARCHIVE_LIBRARY)
      set(ARCHIVE_FOUND TRUE)
      set(ARCHIVE_INCLUDE_DIRS ${ARCHIVE_INCLUDE_DIR})
      set(ARCHIVE_LIBRARIES ${ARCHIVE_LIBRARY})
    endif()
  endif()

  if(NOT CURL_FOUND)
    message(FATAL_ERROR "Could not find libcurl. Set CURL_INCLUDE_DIR and CURL_LIBRARY.")
  endif()
  if(NOT ARCHIVE_FOUND)
    message(FATAL_ERROR "Could not find libarchive. Set ARCHIVE_INCLUDE_DIR and ARCHIVE_LIBRARY.")
  endif()

  target_include_directories(vent PRIVATE ${ARCHIVE_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS})
  target_compile_definitions(vent PRIVATE CURL_STATICLIB LIBARCHIVE_STATIC)
  target_link_directories(vent PRIVATE ${ARCHIVE_LIBRARY_DIRS} ${CURL_LIBRARY_DIRS})
  target_link_libraries(vent PRIVATE ${ARCHIVE_LIBRARIES} ${CURL_LIBRARIES})
  target_link_libraries(vent PRIVATE -lbcrypt -lcrypt32 -lws2_32 -ladvapi32)
else()
  find_package(PkgConfig REQUIRED)
  find_package(CURL REQUIRED)
  pkg_check_modules(ARCHIVE REQUIRED libarchive)
  target_link_libraries(vent PRIVATE CURL::libcurl ${ARCHIVE_LIBRARIES})
  target_include_directories(vent PRIVATE ${ARCHIVE_INCLUDE_DIRS})
  target_compile_options(vent PRIVATE ${ARCHIVE_CFLAGS_OTHER})
endif()

target_include_directories(vent PRIVATE
    ${PROJECT_SOURCE_DIR}/include/
    ${PROJECT_SOURCE_DIR}/external/
)

if(NOT WIN32)
  target_compile_options(vent PRIVATE -Wno-unused-result)
endif()

target_compile_definitions(vent PRIVATE
    VENT_STD_SOURCE="${PROJECT_SOURCE_DIR}/std"
    VENT_STD_REPO_URL="https://github.com/grafmorkov/vent-std"
)

if(WIN32)
  target_link_libraries(vent PRIVATE -lshlwapi)
endif()

install(TARGETS vent
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
