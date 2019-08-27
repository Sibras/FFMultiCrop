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

# Update the git submodule automatically
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    RESULT_VARIABLE GIT_SUBMOD_RESULT)
    if(NOT GIT_SUBMOD_RESULT EQUAL "0")
        message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
    endif()
endif()

set(FFFR_BUILD_SHARED_LIBS OFF CACHE BOOL "Build libraries as shared as opposed to static" FORCE)
add_subdirectory(FFFrameReader)

# Shared/Dynamic or Static library?
option(FFMC_BUILD_SHARED_LIBS "Build libraries as shared as opposed to static" ON)

# Build test related programs?
option(FFMC_BUILD_TESTING "Create test programs" OFF)

# Build python bindings?
option(FFMC_BUILD_PYTHON_BINDING "Create python bindigs" OFF)

# Default to a release build if desired configuration is not specified.
if(NOT CMAKE_CONFIGURATION_TYPES)
    if(NOT CMAKE_BUILD_TYPE)
        message(STATUS "Setting build type to 'Release' as none was specified.")
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY VALUE "Release")
    endif()
endif()

set(FFMC_SOURCES_EXPORT
    ${PROJECT_BINARY_DIR}/FFMCExports.h
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

include(GNUInstallDirs)
set_target_properties(FfMultiCrop PROPERTIES
    FRAMEWORK ON
    SOVERSION 0
    VERSION ${PROJECT_VERSION}
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}"
)

# Detect IPO support
include(CheckIPOSupported)
check_ipo_supported(RESULT BUILD_IPO OUTPUT IPO_ERROR)
if(BUILD_IPO)
set_target_properties(FfMultiCrop PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION TRUE
)
endif()

# Setup export symbol properties
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
include(GenerateExportHeader)
generate_export_header(FfMultiCrop
    EXPORT_FILE_NAME ${FFMC_SOURCES_EXPORT}
)

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

target_include_directories(FfMultiCrop
    PUBLIC ${PROJECT_SOURCE_DIR}/include
    PUBLIC ${PROJECT_BINARY_DIR}
    PRIVATE ${AVUTIL_INCLUDE_DIR}
)
target_link_libraries(FfMultiCrop
    PRIVATE ${AVUTIL_LIBRARY}
    PUBLIC FfFrameReader
)

if("${CMAKE_INSTALL_PREFIX}" STREQUAL "")
    message("Installing into source folder")
    # Temp set the install location to the source location
    set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install CACHE PATH "..." FORCE)
endif()

# Install targets.
install(TARGETS FfMultiCrop
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Add tests
if(FFMC_BUILD_TESTING)
    enable_testing()
    find_package(GTest REQUIRED)
    include(GoogleTest)

    add_executable(FFMCTest 
        test/FFMCTest.cpp
    )

    target_include_directories(FFMCTest PRIVATE
        "${FfFrameReader_SOURCE_DIR}/test"
    )

    target_link_libraries(FFMCTest
        PRIVATE FfMultiCrop
        PRIVATE GTest::GTest
        PRIVATE GTest::Main
        PRIVATE FfFrameReader
    )

    set_target_properties(FFMCTest PROPERTIES
        EXCLUDE_FROM_ALL true
        VS_DEBUGGER_WORKING_DIRECTORY "${FfFrameReader_SOURCE_DIR}/test"
    )

    add_dependencies(FFMCTest FfMultiCrop)

    gtest_discover_tests(FFMCTest
        WORKING_DIRECTORY "${FfFrameReader_SOURCE_DIR}/test"
    )
endif()

# Add python bindings
if(FFMC_BUILD_PYTHON_BINDING)
    find_package(pybind11 REQUIRED)
    
    pybind11_add_module(pyMultiCrop python/FFMCPybind11.cpp)

    configure_file(python/__init__.py.in "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}/pyMultiCrop/__init__.py")

    configure_file(python/setup.py.in "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}/setup.py")

    set_target_properties(pyMultiCrop PROPERTIES
        FRAMEWORK ON
        SOVERSION 0
        VERSION ${PROJECT_VERSION}
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}/pyMultiCrop"
    )

    foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
        set_target_properties(pyMultiCrop PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}/pyMultiCrop")
    endforeach()

    target_link_libraries(pyMultiCrop
        PRIVATE FfMultiCrop
        PRIVATE FfFrameReader
    )

    add_dependencies(pyMultiCrop FfMultiCrop)
endif()
