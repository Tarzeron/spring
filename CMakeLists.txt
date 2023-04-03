cmake_minimum_required(VERSION 3.3)


set(CMAKE_MODULES_SPRING "${CMAKE_CURRENT_SOURCE_DIR}/rts/build/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_MODULES_SPRING}")

project(Spring)

# dummy so that qtcreator shows all files (even those of windows)
if(CMAKE_VERSION VERSION_GREATER 3.1.2)
	add_library(platform_sources EXCLUDE_FROM_ALL "")
endif ()

# includes rts/build/cmake/Util.cmake & rts/build/cmake/PreferStaticLibs.cmake
include(Util)
include(PreferStaticLibs)

# Tell windows to pass arguments around in @response files
# (32k arg limit workaround)
if    (WIN32)
	set(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS 1)
	set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS 1)
endif ()


# XCode
if    (APPLE)
	option(MACOSX_BUNDLE "Compile spring to work as a Bundle.app" TRUE)
	add_definitions(${PIC_FLAG})
	if    (MACOSX_BUNDLE)
		add_definitions(-DMACOSX_BUNDLE)
	endif ()
else ()
	set(MACOSX_BUNDLE FALSE)
endif ()

### Compiler flags and defines based on build type
include(TestCXXFlags)

## 32bit or 64bit?
set(MARCH_FLAG ${MARCH} CACHE STRING "CPU optimization (use i686 for generic optimization)")
if    (CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(MARCH_BITS 64 CACHE INTERNAL "" FORCE)
else ()
	set(MARCH_BITS 32 CACHE INTERNAL "" FORCE)
endif ()

message(STATUS "Building Spring on ${MARCH_BITS}bit environment")
set(BUILD_BITS "${MARCH_BITS}" CACHE STRING "Target arch machine type")
message(STATUS "Targetting ${BUILD_BITS}bit")



# Spring Related USE-Flags
set(LOG_LEVEL "DEFAULT" CACHE STRING
		"The minimum log level.
		Log messages below this level
		will not be compiled into the binary.
		Available levels:
		* DEFAULT (DEBUG on debug builds, INFO on release builds)
		* DEBUG (log everything)
		* INFO
		* WARNING (only use this if you know what you are doing!)")
if    (NOT "${LOG_LEVEL}" STREQUAL "DEFAULT")
	add_definitions(-D_LOG_LEVEL_MIN=LOG_LEVEL_${LOG_LEVEL})
endif ()

option(SYNCCHECK "Check sync during gameplay (Required for online play!)" TRUE)
if    (SYNCCHECK)
	add_definitions(-DSYNCCHECK)
endif ()

option(NO_CREG "Disable creg support" FALSE)
if    (NO_CREG)
	add_definitions(-DNOT_USING_CREG)
endif ()

option(NO_SOUND "No sound output support" FALSE)


find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2>=2)
pkg_check_modules(OPENGL REQUIRED opengl)
pkg_check_modules(GLEW REQUIRED glew>=1.5.1)
pkg_check_modules(IL REQUIRED IL)



# Sub-parts of the build system may add dependencies to this target
add_custom_target(generateSources)


#add_definitions(-DSTREFLOP_SOFT)
add_definitions(-DSTREFLOP_SSE)


### Where the other CMakeLists.txt are

# As the libs are required by all the other sub-projects, we add them first
add_subdirectory(rts/lib)

#if    (NOT "${AI_TYPES}" STREQUAL "NONE")
#	add_subdirectory(AI)
#endif ()

# As tools/unitsync/ depends on sources from here, we add this one first
add_subdirectory(rts/System)

# additional tools
add_subdirectory(tools)

# man pages & user-docs
add_subdirectory(doc)

### Handle game content
### - Pack base-files
### - Install base-files
### - Install freedesktop stuff
### - Install mingw DLLs
add_subdirectory(cont)

# As rts/builds/* depends on most other stuff, we add this one as last
add_subdirectory(rts)

# Unit tests
# this has to be in root CMakeLists.txt
enable_testing()
add_subdirectory(test)
