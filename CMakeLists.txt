cmake_minimum_required(VERSION 3.21)
project(RePlayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(REPLAYER_ENABLE_STATIC_CRT "Link the MSVC runtime statically in Release builds." OFF)
if (MSVC AND REPLAYER_ENABLE_STATIC_CRT)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)
include(CTest)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

file(GLOB_RECURSE REPLAYER_AUDIO_SOURCES CONFIGURE_DEPENDS audio/*.cpp)
file(GLOB_RECURSE REPLAYER_CORE_SOURCES CONFIGURE_DEPENDS core/*.cpp)
file(GLOB_RECURSE REPLAYER_RECORDING_SOURCES CONFIGURE_DEPENDS recording/*.cpp)
file(GLOB_RECURSE REPLAYER_STATE_SOURCES CONFIGURE_DEPENDS state/*.cpp)
file(GLOB_RECURSE REPLAYER_SUBTITLE_SOURCES CONFIGURE_DEPENDS subtitles/*.cpp)
file(GLOB_RECURSE REPLAYER_UI_SOURCES CONFIGURE_DEPENDS ui/*.cpp)
file(GLOB_RECURSE REPLAYER_UTILS_SOURCES CONFIGURE_DEPENDS utils/*.cpp)

add_library(replayer_core
    ${REPLAYER_AUDIO_SOURCES}
    ${REPLAYER_CORE_SOURCES}
    ${REPLAYER_RECORDING_SOURCES}
    ${REPLAYER_STATE_SOURCES}
    ${REPLAYER_SUBTITLE_SOURCES}
    ${REPLAYER_UTILS_SOURCES}
)

target_include_directories(replayer_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(replayer_core PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

if (MSVC)
    target_compile_options(replayer_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(replayer_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(replayer_core
    PUBLIC
        winmm
        ole32
        avrt
        shlwapi
)

add_executable(RePlayer WIN32
    src/main.cpp
    ${REPLAYER_UI_SOURCES}
)

target_include_directories(RePlayer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(RePlayer PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

if (MSVC)
    target_compile_options(RePlayer PRIVATE /W4 /permissive-)
else()
    target_compile_options(RePlayer PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(RePlayer
    PRIVATE
        replayer_core
        comctl32
        comdlg32
)

enable_testing()

file(GLOB_RECURSE REPLAYER_TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
add_executable(replayer_tests ${REPLAYER_TEST_SOURCES})
target_include_directories(replayer_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(replayer_tests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(replayer_tests PRIVATE replayer_core GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(replayer_tests DISCOVERY_MODE PRE_TEST)

install(TARGETS RePlayer RUNTIME DESTINATION .)
install(DIRECTORY DESTINATION recordings)
install(DIRECTORY DESTINATION logs)
install(DIRECTORY DESTINATION config)

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_FILE_NAME "RePlayer-windows-x64")
include(CPack)
