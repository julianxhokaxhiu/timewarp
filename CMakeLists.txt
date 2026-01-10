#*****************************************************************************#
#    Copyright (C) 2026 Julian Xhokaxhiu                                      #
#                                                                             #
#    This file is part of timewarp                                            #
#                                                                             #
#    timewarp is free software: you can redistribute it and/or modify         #
#    it under the terms of the GNU General Public License as published by     #
#    the Free Software Foundation, either version 3 of the License            #
#                                                                             #
#    timewarp is distributed in the hope that it will be useful,              #
#    but WITHOUT ANY WARRANTY; without even the implied warranty of           #
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            #
#    GNU General Public License for more details.                             #
#*****************************************************************************#

cmake_minimum_required(VERSION 3.25)
cmake_policy(SET CMP0091 NEW)

if(NOT DEFINED CMAKE_BUILD_TYPE)
  message(FATAL_ERROR "CMAKE_BUILD_TYPE must be set to continue building with cmake. \nExample: Add -DCMAKE_BUILD_TYPE=Release to your cmake command line.")
endif()

set(VCPKG_DISABLE_COMPILER_TRACKING 1)
set(VCPKG_INSTALL_OPTIONS "--clean-after-build" "--allow-unsupported")

set(CMAKE_SHARED_LINKER_FLAGS
  "${CMAKE_SHARED_LINKER_FLAGS} /NODEFAULTLIB:MSVCRT /NODEFAULTLIB:MSVCRTD /DEBUG:FULL /FORCE:MULTIPLE /IGNORE:4006,4075,4099,4217"
)

if(CMAKE_BUILD_TYPE MATCHES Debug)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /NODEFAULTLIB:LIBCMT")
else()
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /NODEFAULTLIB:LIBCMTD")
endif()

project ("timewarp")

find_path(DETOURS_INCLUDE_DIRS "detours/detours.h")
find_library(DETOURS_LIBRARY detours REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)

set(RELEASE_NAME "timewarp")

if(_DLL_VERSION STREQUAL "devel" OR _DLL_VERSION MATCHES "-")
  set(_DLL_RCVERSION "0,0,0,0")
  set(_DLL_RCSTRVERSION "0.0.0.0")
  set(PATCH_COLLECT_DUPLICATES 1)
else()
  string(REPLACE "." "," _DLL_RCVERSION ${_DLL_VERSION})
  set(_DLL_RCSTRVERSION ${_DLL_VERSION})
endif()

# Include all the source code files
file(GLOB_RECURSE source_files "${CMAKE_SOURCE_DIR}/src/*.cpp")

configure_file(misc/version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/version.rc @ONLY)

add_library(${RELEASE_NAME} SHARED
  ${source_files}
  ${CMAKE_CURRENT_BINARY_DIR}/version.rc
)

target_compile_options(
  ${RELEASE_NAME}
  PRIVATE /DVERSION="${_DLL_VERSION}"
  PRIVATE /D_CRT_SECURE_NO_WARNINGS
  PRIVATE /DNOMINMAX
  PRIVATE /Zc:strictStrings-
  PRIVATE /Zc:__cplusplus
  PRIVATE /Zc:preprocessor
  PRIVATE /Qpar
  PRIVATE /MP
)
target_compile_features(
  ${RELEASE_NAME}
  PRIVATE cxx_std_20
)
target_include_directories(
  ${RELEASE_NAME}
  PRIVATE ${DETOURS_INCLUDE_DIRS}
)
target_link_libraries(
  ${RELEASE_NAME}
  PRIVATE tomlplusplus::tomlplusplus
  PRIVATE ${DETOURS_LIBRARY}
)
target_link_options(
  ${RELEASE_NAME}
  PRIVATE /PDBALTPATH:${RELEASE_NAME}.pdb
  PRIVATE /DEF:${CMAKE_SOURCE_DIR}/misc/${RELEASE_NAME}.def
)

# INSTALL
add_custom_command(
  TARGET ${RELEASE_NAME}
  POST_BUILD
  COMMAND echo Preparing ${RELEASE_NAME} ${_DLL_VERSION} release...
  # ensure bin directory exists
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/bin
  # License
  COMMAND ${CMAKE_COMMAND} -E copy
          ${CMAKE_CURRENT_SOURCE_DIR}/COPYING.TXT
          ${CMAKE_BINARY_DIR}/bin
  # .dll
  COMMAND ${CMAKE_COMMAND} -E copy
          ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/${RELEASE_NAME}.dll
          ${CMAKE_BINARY_DIR}/bin
  # .toml
  COMMAND ${CMAKE_COMMAND} -E copy
          ${CMAKE_CURRENT_SOURCE_DIR}/misc/${RELEASE_NAME}.toml
          ${CMAKE_BINARY_DIR}/bin
)

# CPU INFO
add_custom_command(
  TARGET ${RELEASE_NAME}
  PRE_BUILD
  # Get CPU info
  COMMAND
    ${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/tools/cpuinfo/cpu-info
  # Get CPU ISA supported instructions
  COMMAND
    ${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/tools/cpuinfo/isa-info
)
