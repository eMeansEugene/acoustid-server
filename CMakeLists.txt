cmake_minimum_required(VERSION 3.20)
project(acoustid_server CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# pocketfft — header-only, MIT. Нет CMake-таргета в репозитории,
# поэтому оборачиваем сами в INTERFACE-библиотеку.
FetchContent_Declare(
        pocketfft
        GIT_REPOSITORY https://github.com/mreineck/pocketfft.git
        GIT_TAG c90e55b3d529f8efa40ed01a20de22405f45fc65 # branch: cpp, 2026-06-30
)
FetchContent_MakeAvailable(pocketfft)

add_library(pocketfft INTERFACE)
target_include_directories(pocketfft INTERFACE ${pocketfft_SOURCE_DIR})

# --- afs_core: вычислительное ядро (DSP) ---
add_library(afs_core
        src/core/fft_engine.cpp
        src/core/fft_engine.h
        tests/core/fft_engine_test.cpp
)
target_include_directories(afs_core PUBLIC include)
target_link_libraries(afs_core PUBLIC pocketfft)

# --- Тесты ---
option(AFS_BUILD_TESTS "Build unit tests" ON)
if(AFS_BUILD_TESTS)
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.15.2
    )
    # Отключаем установку gtest вместе с проектом
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_executable(afs_core_tests
            tests/core/fft_engine_test.cpp
    )
    target_link_libraries(afs_core_tests PRIVATE afs_core GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(afs_core_tests)
endif()