# 
#  Copyright 2012 Technical University of Denmark, DTU Compute.
#  All rights reserved.
#
#  This file is part of the Patmos Simulator.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions are met:
#
#      1. Redistributions of source code must retain the above copyright notice,
#         this list of conditions and the following disclaimer.
#
#      2. Redistributions in binary form must reproduce the above copyright
#         notice, this list of conditions and the following disclaimer in the
#         documentation and/or other materials provided with the distribution.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER ``AS IS'' AND ANY EXPRESS
#   OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
#   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
#   NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
#   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
#   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
#   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
#   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
#   THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#   The views and conclusions contained in the software and documentation are
#   those of the authors and should not be interpreted as representing official
#   policies, either expressed or implied, of the copyright holder.


project(patmos)

cmake_minimum_required(VERSION 2.8)

option(USE_RAMULATOR "USE_RAMULATOR" OFF)

# Build release with debug by default
if (NOT CMAKE_BUILD_TYPE)
  message(STATUS "No build type selected, defaulting to RelWithDebInfo")
  set(CMAKE_BUILD_TYPE "RelWithDebInfo")
else()
  message(STATUS "Build type set to: ${CMAKE_BUILD_TYPE}")
endif()

# Find boost library
include(FindBoost)
set(Boost_USE_STATIC_LIBS ON)
find_package( Boost 1.46.0 COMPONENTS program_options REQUIRED)

# Find libelf
find_library(ELF elf)
if (NOT ELF)
  message(FATAL_ERROR "libelf library not found.")
endif()

find_path(ELF_INCLUDE_DIRS gelf.h PATH_SUFFIXES libelf)
if (NOT ELF_INCLUDE_DIRS)
  message(FATAL_ERROR "libelf headers not found.")
endif()

include_directories(include ./ ${Boost_INCLUDE_DIRS} ${ELF_INCLUDE_DIRS})

# make sure the boost templates compile
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-depth-1024")

# enable testing using ctest
enable_testing()

# add sub-directories
if(USE_RAMULATOR)
  add_definitions("-DRAMULATOR")
  add_subdirectory(ramulator)
endif()
add_subdirectory(src)
add_subdirectory(tests)

# Save the target triple in a variable
execute_process( COMMAND gcc -dumpmachine OUTPUT_VARIABLE TARGET_TRIPLE OUTPUT_STRIP_TRAILING_WHITESPACE )
set( PROJECT_NAME "patmos-simulator")
set( PACKAGE_TAR "${PROJECT_NAME}-${TARGET_TRIPLE}.tar")
set( PACKAGE_TAR_GZ "${PACKAGE_TAR}.gz")
set( PACKAGE_INFO_FILE "${PROJECT_NAME}-info.yml")
set( PACKAGE_BINARIES "src/pasim" "src/pacheck" "src/paasm" "src/padasm" )
if (APPLE) 
	# Mac usually has BSD tar
	set(TRANSFORM_FLAG "-s")
	set(TRANSFORM_STRING "/^src/bin/")
else()
	# By default, assume using GNU tar.
	set(TRANSFORM_FLAG "--transform")
	set(TRANSFORM_STRING "s,^src,bin,")
endif()
# Build release tarball containing binaries and metadata
add_custom_command(
	OUTPUT ${PACKAGE_TAR_GZ} 
		# We add all generated files to the list of outputs to ensure that cleaning will remove them.
		${PACKAGE_TAR} ${PACKAGE_INFO_FILE}
	
	# Package binaries
	COMMAND tar -cf ${PACKAGE_TAR} 
		# Rename "src/*" to "bin/*"
		${TRANSFORM_FLAG} ${TRANSFORM_STRING} ${PACKAGE_BINARIES}
	
	# Build YAML info file
	COMMAND ${CMAKE_COMMAND} -E echo "name: ${PROJECT_NAME}" > ${PACKAGE_INFO_FILE}
	COMMAND ${CMAKE_COMMAND} -E echo "target: ${TARGET_TRIPLE}" >> ${PACKAGE_INFO_FILE}
	COMMAND ${CMAKE_COMMAND} -E echo_append "version: " >> ${PACKAGE_INFO_FILE}
	COMMAND git describe --tags >> ${PACKAGE_INFO_FILE}
	COMMAND ${CMAKE_COMMAND} -E echo_append "commit: " >> ${PACKAGE_INFO_FILE}
	COMMAND git rev-parse HEAD >> ${PACKAGE_INFO_FILE}
	COMMAND ${CMAKE_COMMAND} -E echo "files:" >> ${PACKAGE_INFO_FILE}
	COMMAND tar -tf ${PACKAGE_TAR} | sed "s/^/- /" >> ${PACKAGE_INFO_FILE}
	
	# Add the info file to the package
	COMMAND tar -rf ${PACKAGE_TAR} ${PACKAGE_INFO_FILE}
	
	# Compress the tar
	COMMAND gzip -9 < ${PACKAGE_TAR} > ${PACKAGE_TAR_GZ}
	
	DEPENDS ${PACKAGE_BINARIES}
)
# Rename release tarball target to something better.
add_custom_target(box DEPENDS ${PACKAGE_TAR_GZ})
