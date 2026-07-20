find_package(Doxygen REQUIRED)

include(FetchContent)

find_package(Doxygen REQUIRED COMPONENTS dot)

FetchContent_Declare(
        doxygen-awesome-css
        GIT_REPOSITORY https://github.com/jothepro/doxygen-awesome-css.git
        GIT_TAG v2.4.2
)

FetchContent_MakeAvailable(doxygen-awesome-css)

FetchContent_GetProperties(
        doxygen-awesome-css
        SOURCE_DIR DOXYGEN_AWESOME_DIR
)

set(DOXYFILE_IN
        ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
)

set(DOXYFILE_OUT
        ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
)

configure_file(
        ${DOXYFILE_IN}
        ${DOXYFILE_OUT}
        @ONLY
)

add_custom_target(doc
        COMMAND
        ${DOXYGEN_EXECUTABLE}
        ${DOXYFILE_OUT}

        WORKING_DIRECTORY
        ${CMAKE_CURRENT_BINARY_DIR}

        COMMENT
        "Generating Doxygen documentation"
)