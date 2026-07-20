cmake_minimum_required(VERSION 3.20)
project(acoustid_server
        VERSION 0.1
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# --- Внешние зависимости ---

FetchContent_Declare(
        pocketfft
        GIT_REPOSITORY https://github.com/mreineck/pocketfft.git
        GIT_TAG c90e55b3d529f8efa40ed01a20de22405f45fc65
)
FetchContent_MakeAvailable(pocketfft)
add_library(pocketfft INTERFACE)
target_include_directories(pocketfft INTERFACE ${pocketfft_SOURCE_DIR})

FetchContent_Declare(
        dr_libs
        GIT_REPOSITORY https://github.com/mackron/dr_libs.git
        GIT_TAG da35f9d6c7374a95353fd1df1d394d44ab66cf01
)
FetchContent_MakeAvailable(dr_libs)
add_library(dr_wav INTERFACE)
target_include_directories(dr_wav INTERFACE ${dr_libs_SOURCE_DIR})

FetchContent_Declare(
        minimp3
        GIT_REPOSITORY https://github.com/lieff/minimp3.git
        GIT_TAG afb604c06bc8beb145fecd42c0ceb5bda8795144
)
FetchContent_MakeAvailable(minimp3)
add_library(minimp3 INTERFACE)
target_include_directories(minimp3 INTERFACE ${minimp3_SOURCE_DIR})

FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
        crow
        GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
        GIT_TAG v1.2.0
)
set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(crow)

find_package(SQLite3 REQUIRED)

# --- Библиотеки проекта ---

add_library(aid_core
        src/core/fft_engine.cpp
        src/core/fft_engine.h
        src/core/peak_extractor.cpp
        src/core/peak_extractor.h
        src/core/hash_generator.cpp
        src/core/hash_generator.h
        src/core/voting_engine.cpp
        src/core/voting_engine.h
        src/core/audio_fingerprint_engine.cpp
        src/core/audio_fingerprint_engine.h
)
target_include_directories(aid_core PUBLIC src)
target_link_libraries(aid_core PUBLIC pocketfft)

add_library(aid_audio
        src/audio/audio_decoder.cpp
        src/audio/audio_decoder.h
)
target_include_directories(aid_audio PUBLIC src)
target_link_libraries(aid_audio PUBLIC minimp3 dr_wav)

add_library(aid_storage
        src/storage/sqlite_repository.cpp
        src/storage/sqlite_repository.h
)
target_include_directories(aid_storage PUBLIC src)
target_link_libraries(aid_storage PUBLIC aid_core SQLite::SQLite3)

add_library(aid_domain
        src/domain/indexing_service.cpp
        src/domain/indexing_service.h
        src/domain/matching_service.cpp
        src/domain/matching_service.h
)
target_include_directories(aid_domain PUBLIC src)
target_link_libraries(aid_domain PUBLIC aid_core aid_audio)

add_library(aid_server
        src/server/task_queue.cpp
        src/server/task_queue.h
        src/server/task_registry.cpp
        src/server/task_registry.h
        src/server/worker_pool.cpp
        src/server/worker_pool.h
        src/server/http_server.cpp
        src/server/http_server.h
        src/server/task.h
        src/server/server_config.h
)
target_include_directories(aid_server PUBLIC src)
target_link_libraries(aid_server PUBLIC aid_domain aid_storage Crow::Crow nlohmann_json::nlohmann_json)

# --- Исполняемые файлы ---

add_executable(acoustid_server src/main_server.cpp src/app_config.cpp)
target_link_libraries(acoustid_server PRIVATE aid_server nlohmann_json::nlohmann_json)

add_executable(indexer src/main_indexer.cpp src/app_config.cpp)
target_link_libraries(indexer PRIVATE aid_domain aid_storage nlohmann_json::nlohmann_json)

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
    )
    target_link_libraries(aid_audio_tests PRIVATE aid_audio GTest::gtest_main)
    gtest_discover_tests(aid_audio_tests)

    add_executable(aid_storage_tests
            tests/storage/sqlite_repository_test.cpp
    )
    target_link_libraries(aid_storage_tests PRIVATE aid_storage GTest::gtest_main)
    gtest_discover_tests(aid_storage_tests)

    add_executable(aid_domain_tests
            tests/domain/domain_integration_test.cpp
    )
    target_link_libraries(aid_domain_tests PRIVATE aid_domain aid_storage GTest::gtest_main)
    gtest_discover_tests(aid_domain_tests)

    add_executable(aid_server_tests
            tests/server/server_test.cpp
    )
    target_link_libraries(aid_server_tests PRIVATE aid_server GTest::gtest_main)
    gtest_discover_tests(aid_server_tests)
endif()



include(GNUInstallDirs)

install(TARGETS acoustid_server
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(TARGETS indexer
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})


set(CPACK_GENERATOR "DEB")

set(CPACK_PACKAGE_NAME "acoustid-server")
set(CPACK_PACKAGE_VENDOR "eForEugene")
set(CPACK_PACKAGE_CONTACT "evgeniy.retc@gmail.com")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Eugene <evgeniy.retc@gmail.com>")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

set(CPACK_PACKAGE_VERSION_MAJOR 0)
set(CPACK_PACKAGE_VERSION_MINOR 0)
set(CPACK_PACKAGE_VERSION_PATCH ${PATCH_VERSION})

include(CPack)

