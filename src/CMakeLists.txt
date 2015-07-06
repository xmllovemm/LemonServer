# the whole CMakeLists.txt for project

# CMake version check
cmake_minimum_required(VERSION 2.8)

# project info
project(ics-client)

# print
message("Project src dir:${PROJECT_SOURCE_DIR}")
message("Project bin dir:${PROJECT_BINARY_DIR}")

# set Debug/Release modle
SET(CMAKE_BUILD_TYPE Debug)

# show the detail info when make
# set(CMAKE_VERBOSE_MAKEFILE on)

# set compiler option:CMAKE_C_FLAGS_DEBUG = c,CMAKE_CXX_FLAGS_DEBUG = cxx
SET(CMAKE_C_FLAGS_DEBUG "-g -Wall")
SET(CMAKE_C_FLAGS_RELEASE "-O2 -Wall")
SET(CMAKE_CXX_FLAGS_DEBUG "-g -std=c++0x -Wall")
SET(CMAKE_CXX_FLAGS_RELEASE "-O2 -std=c++0x -Wall")

# set include directories
include_directories(
	${PROJECT_SOURCE_DIR}
)

# set lib directories
link_directories(
	/usr/lib64
	/usr/local/lib
)

# target name
set(target "ics-client")


# set output bin directory
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

# set output lib directory
SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)


# source file
aux_source_directory(. SRC_FILES)

# exec
add_executable(${target} ${SRC_FILES})

# link dll
target_link_libraries(${target} pthread)



# -------------useful function------------------ #
# set macro
# add_definitions("-DDEBUG")


# link dll
# target_link_libraries(comm pthread)

# set compiler option:CMAKE_C_FLAGS_DEBUG = c,CMAKE_CXX_FLAGS_DEBUG = cxx
# SET(CMAKE_C_FLAGS_DEBUG "-g -Wall")
# SET(CMAKE_CXX_FLAGS_DEBUG "-g -Wall")
