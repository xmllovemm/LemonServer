# the whole CMakeLists.txt for project

# CMake version check
cmake_minimum_required(VERSION 2.8)

# project info
project(ics-client CXX)

# print
message("Project src dir:${PROJECT_SOURCE_DIR}")
message("Project bin dir:${PROJECT_BINARY_DIR}")

# set Debug/Release modle
set(CMAKE_BUILD_TYPE Debug)

add_definitions("-DTIXML_USE_STL")

# show the detail info when make
# set(CMAKE_VERBOSE_MAKEFILE on)

# set compiler option:CMAKE_C_FLAGS_DEBUG = c,CMAKE_CXX_FLAGS_DEBUG = cxx
set(CMAKE_C_COMPILER "/usr/local/gcc-5.2/bin/gcc")
set(CMAKE_C_FLAGS_DEBUG "-g -Wall")
set(CMAKE_C_FLAGS_RELEASE "-O2 -Wall")

set(CMAKE_CXX_COMPILER "/usr/local/gcc-5.2/bin/g++")
set(CMAKE_CXX_FLAGS_DEBUG "-g -std=c++14 -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -std=c++14 -Wall")

message("Project CMAKE_CXX_COMPILER:${CMAKE_CXX_COMPILER}")

# set include directories
include_directories(
	${PROJECT_SOURCE_DIR}/src
)

# set lib directories
link_directories(
	/usr/local/gcc-5.2/lib
)

# target name
set(target "ics-client")


# set output bin directory
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

# set output lib directory
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)


# source file
aux_source_directory(./src SRC_FILES)



# exec
add_executable(${target} ${SRC_FILES})

# link dll
target_link_libraries(${target} pthread)
target_link_libraries(${target} odbc)



# -------------useful function------------------ #
# set macro
# add_definitions("-DDEBUG")


# link dll
# target_link_libraries(comm pthread)

# set compiler option:CMAKE_C_FLAGS_DEBUG = c,CMAKE_CXX_FLAGS_DEBUG = cxx
# set(CMAKE_C_FLAGS_DEBUG "-g -Wall")
# set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall")
