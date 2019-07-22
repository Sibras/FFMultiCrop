# CMakeList.txt : CMake project for FFFR, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10.2)

# Use vcpkg if available
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    file(TO_CMAKE_PATH "$ENV{VCPKG_ROOT}" ENV_VCPKG_ROOT)
    set(CMAKE_TOOLCHAIN_FILE "${ENV_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "")
endif()

project(FfMultiCrop
    VERSION 0.1
    DESCRIPTION "FFmpeg based library for creating multiple cropped encodes of a video source"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build position independent code.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Disable C and C++ compiler specific extensions.
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

# Setup export symbol properties
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

# Defines the CMAKE_INSTALL_LIBDIR, CMAKE_INSTALL_BINDIR and many other useful macros.
include(GNUInstallDirs)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")

# Differentiate between release/debug builds
if(MSVC)
    set(CMAKE_DEBUG_POSTFIX "d")
endif()

set(FFFR_BUILD_SHARED_LIBS OFF CACHE BOOL "Build libraries as shared as opposed to static" FORCE)
add_subdirectory(FFFrameReader)

# Shared/Dynamic or Static library?
option(FFMC_BUILD_SHARED_LIBS "Build libraries as shared as opposed to static" ON)

# Build test related programs?
option(FFMC_BUILD_TESTING "Create test programs" OFF)

# Default to a release build if desired configuration is not specified.
if(NOT CMAKE_CONFIGURATION_TYPES)
    if(NOT CMAKE_BUILD_TYPE)
        message(STATUS "Setting build type to 'Release' as none was specified.")
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY VALUE "Release")
    endif()
endif()

# Detect IPO support
include(CheckIPOSupported)
check_ipo_supported(RESULT BUILD_IPO OUTPUT IPO_ERROR)

include(GenerateExportHeader)

set(FFMC_SOURCES_EXPORT
    ${PROJECT_BINARY_DIR}/ffmulticrop_export.h
)

set(FFMC_SOURCES
    source/FFMC.cpp
    ${FFMC_SOURCES_EXPORT}
)

set(FFMC_HEADERS
    include/FFMultiCrop.h
)

# Add source to this project's executable.
if(FFMC_BUILD_SHARED_LIBS)
    add_library(FfMultiCrop
        SHARED
        ${FFMC_SOURCES}
        ${FFMC_HEADERS}
    )
else()
    add_library(FfMultiCrop
        STATIC
        ${FFMC_SOURCES}
        ${FFMC_HEADERS}
    )
endif()

set_target_properties(FfMultiCrop PROPERTIES
    FRAMEWORK ON
    SOVERSION 0
    VERSION ${PROJECT_VERSION}
    PUBLIC_HEADER "${FFMC_HEADERS}"
)

if(BUILD_IPO)
set_target_properties(FfMultiCrop PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION TRUE
)
endif()

generate_export_header(FfMultiCrop)

add_dependencies(FfMultiCrop FfFrameReader)

# Enable all compile warnings
if(MSVC)
  target_compile_options(FfMultiCrop PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4 /WX /experimental:external /external:anglebrackets /external:W3>)
else()
  target_compile_options(FfMultiCrop PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -pedantic -Werror>)
endif()

# Find the required FFmpeg libraries
find_path(AVUTIL_INCLUDE_DIR NAMES libavutil/avutil.h)
find_library(AVUTIL_LIBRARY NAMES avutil)

target_include_directories(FfMultiCrop PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}
    ${AVUTIL_INCLUDE_DIR}
    FfFrameReader/include
    ${FfFrameReader_BINARY_DIR}
)
target_link_libraries(FfMultiCrop
    PRIVATE ${AVUTIL_LIBRARY}
    PRIVATE FfFrameReader
)

if("${CMAKE_INSTALL_PREFIX}" STREQUAL "")
    message("Installing into source folder")
    # Temp set the install location to the source location
    set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install CACHE PATH "..." FORCE)
endif()

# Install targets.
install(TARGETS FfMultiCrop
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)

# Add tests
if(FFMC_BUILD_TESTING)
    enable_testing()
    find_package(GTest REQUIRED)
    include(GoogleTest)

    set(FFMC_TEST_SOURCES
        test/FFMCTest.cpp
    )

    add_executable(FFMCTest ${FFMC_TEST_SOURCES})

    target_include_directories(FFMCTest PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        FfFrameReader/include
        FfFrameReader/test
        ${FfFrameReader_BINARY_DIR}
        ${PROJECT_BINARY_DIR}
    )

    target_link_libraries(
        FFMCTest
        PRIVATE FfMultiCrop
        PRIVATE GTest::GTest
        PRIVATE GTest::Main
        PRIVATE FfFrameReader
    )

    set_target_properties(
        FFMCTest PROPERTIES
        EXCLUDE_FROM_ALL true
        VS_DEBUGGER_WORKING_DIRECTORY "${FfFrameReader_SOURCE_DIR}/test"
    )

    add_dependencies(FFMCTest FfMultiCrop)

    gtest_discover_tests(FFMCTest
        WORKING_DIRECTORY "${FfFrameReader_SOURCE_DIR}/test"
    )
endif()