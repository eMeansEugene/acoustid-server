cmake_minimum_required(VERSION 3.20)
project(acoustid_server CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# pocketfft — header-only, MIT.
FetchContent_Declare(
        pocketfft
        GIT_REPOSITORY https://github.com/mreineck/pocketfft.git
        GIT_TAG c90e55b3d529f8efa40ed01a20de22405f45fc65
)
FetchContent_MakeAvailable(pocketfft)

add_library(pocketfft INTERFACE)
target_include_directories(pocketfft INTERFACE ${pocketfft_SOURCE_DIR})

# dr_libs — header-only (dr_wav.h), public domain.
FetchContent_Declare(
        dr_libs
        GIT_REPOSITORY https://github.com/mackron/dr_libs.git
        GIT_TAG da35f9d6c7374a95353fd1df1d394d44ab66cf01
)
FetchContent_MakeAvailable(dr_libs)

add_library(dr_wav INTERFACE)
target_include_directories(dr_wav INTERFACE ${dr_libs_SOURCE_DIR})

# minimp3 — header-only, CC0 / MIT.
FetchContent_Declare(
        minimp3
        GIT_REPOSITORY https://github.com/lieff/minimp3.git
        GIT_TAG afb604c06bc8beb145fecd42c0ceb5bda8795144
)
FetchContent_MakeAvailable(minimp3)

add_library(minimp3 INTERFACE)
target_include_directories(minimp3 INTERFACE ${minimp3_SOURCE_DIR})

# --- aid_core: вычислительное ядро (DSP) ---
add_library(aid_core
        src/core/fft_engine.cpp
        src/core/fft_engine.h
        src/core/peak_extractor.cpp
        src/core/peak_extractor.h
        src/core/hash_generator.cpp
        src/core/hash_generator.h
        src/core/voting_engine.cpp
        src/core/voting_engine.h
)
target_include_directories(aid_core PUBLIC src)
target_link_libraries(aid_core PUBLIC pocketfft)

# --- aid_audio: декодирование аудиоформатов ---
add_library(aid_audio
        src/audio/audio_decoder.cpp
        src/audio/audio_decoder.h
)
target_include_directories(aid_audio PUBLIC src)
target_link_libraries(aid_audio PUBLIC minimp3 dr_wav)

# --- Тесты ---
option(AID_BUILD_TESTS "Build unit tests" ON)
if(AID_BUILD_TESTS)
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.15.2
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    include(GoogleTest)

    add_executable(aid_core_tests
            tests/core/fft_engine_test.cpp
            tests/core/peak_extractor_test.cpp
            tests/core/hash_generator_test.cpp
            tests/core/voting_engine_test.cpp
    )
    target_link_libraries(aid_core_tests PRIVATE aid_core GTest::gtest_main)
    gtest_discover_tests(aid_core_tests)

    add_executable(aid_audio_tests
            tests/audio/audio_decoder_test.cpp
            tests/audio/audio_decoder_test.cpp
    )
    target_link_libraries(aid_audio_tests PRIVATE aid_audio GTest::gtest_main)
    gtest_discover_tests(aid_audio_tests)
endif()