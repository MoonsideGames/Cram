cmake_minimum_required(VERSION 2.8.12)
project(Cram C)

option(BUILD_CLI "Build command line executable" ON)
option(BUILD_SHARED_LIBS "Build shared library" OFF)

SET(LIB_MAJOR_VERSION "0")
SET(LIB_MINOR_VERSION "3")
SET(LIB_REVISION "0")
SET(LIB_VERSION "${LIB_MAJOR_VERSION}.${LIB_MINOR_VERSION}.${LIB_REVISION}")

# Build Type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	# By default, we use Release
	message(STATUS "Setting build type to 'Release' as none was specified.")
	set(CMAKE_BUILD_TYPE "Release" CACHE
		STRING "Choose the type of build." FORCE
	)
	# Set the possible values of build type for cmake-gui
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
		STRINGS "Debug" "Release" "RelWithDebInfo"
	)
endif()

# Platform Flags
if(APPLE)
	set(CMAKE_MACOSX_RPATH ON)
	set(CMAKE_OSX_DEPLOYMENT_TARGET 10.9)
	set(LOBJC "objc")
elseif(WIN32)
	# "Cram.dll", not "libCram.dll"
	set(CMAKE_SHARED_LIBRARY_PREFIX "")
endif()
if(UNIX)
	set(CMAKE_SKIP_BUILD_RPATH TRUE)
	set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
	set(CMAKE_INSTALL_RPATH ${BIN_RPATH})
	set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
endif()

if (WIN32)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

file(GLOB SOURCE_FILES
	#Public header
	include/cram.h
	#Source
	lib/stb_image.h
	lib/stb_ds.h
	src/cram.c
)

set(LINKSTYLE PRIVATE)
if(BUILD_SHARED_LIBS)
	set(LINKSTYLE PUBLIC)
endif()

if(BUILD_CLI)
	file(GLOB CLI_SOURCES
		tools/cli/lib/stb_image_write.h
		tools/cli/json_writer.h
		tools/cli/main.c
	)

	if(WIN32)
		list(APPEND CLI_SOURCES tools/cli/lib/dirent.h)
	endif()

	add_executable(cramcli ${CLI_SOURCES})

	if(BUILD_SHARED_LIBS)
		target_link_libraries(cramcli PUBLIC Cram)
	else()
		target_link_libraries(cramcli PRIVATE Cram)
	endif()

	if(WIN32)
		target_include_directories(cramcli PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tools/cli/lib/windows>)
	endif()
endif()

if(BUILD_SHARED_LIBS)
	add_library(Cram SHARED ${SOURCE_FILES})
else()
	add_library(Cram STATIC ${SOURCE_FILES})
endif()

# Build flags
if(NOT MSVC)
	set_property(TARGET Cram PROPERTY COMPILE_FLAGS "-std=gnu99 -Wall -Wno-strict-aliasing -pedantic")
	target_link_libraries(Cram PUBLIC m)
endif()

# includes
target_include_directories(Cram PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tools/cli/lib>
)

# Soname
set_target_properties(Cram PROPERTIES OUTPUT_NAME "Cram"
	VERSION ${LIB_VERSION}
	SOVERSION ${LIB_MAJOR_VERSION}
)
