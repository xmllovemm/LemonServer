# the whole CMakeLists.txt for project

# CMake version check
cmake_minimum_required(VERSION 2.8)

# project info
project(ics-server CXX)

# print
message("Project src dir:${PROJECT_SOURCE_DIR}")
message("Project bin dir:${PROJECT_BINARY_DIR}")

# set cross-compile
# set(CMAKE_SYSTEM_NAME Linux)

# set Debug/Release modle
set(CMAKE_BUILD_TYPE Debug)

add_definitions("-DTIXML_USE_STL")
add_definitions("-DLOG_USE_LOG4CPLUS")

# show the detail info when make
# set(CMAKE_VERBOSE_MAKEFILE on)

# set compiler option:CMAKE_C_FLAGS_DEBUG = c,CMAKE_CXX_FLAGS_DEBUG = cxx
set(CMAKE_C_COMPILER "/usr/local/gcc-5.2/bin/gcc")
#set(CMAKE_C_COMPILER "/usr/bin/gcc")
set(CMAKE_C_FLAGS_DEBUG "-g -Wall")
set(CMAKE_C_FLAGS_RELEASE "-O2 -Wall")

set(CMAKE_CXX_COMPILER "/usr/local/gcc-5.2/bin/g++")
#set(CMAKE_CXX_COMPILER "/usr/bin/g++")
set(CMAKE_CXX_FLAGS_DEBUG "-g -std=c++14 -Wall -gstabs+ -fno-use-linker-plugin -Wno-deprecated-declarations -Wl,-rpath=/usr/local/gcc-5.2 -static-libgcc")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -std=c++14 -Wall -fno-use-linker-plugin -Wno-deprecated-declarations")

message("Project CMAKE_CXX_COMPILER:${CMAKE_CXX_COMPILER}")

# set include directories
include_directories(
	${PROJECT_SOURCE_DIR}/src/module
)

# set lib directories
link_directories(
	/usr/local/gcc-5.2/lib64
	/home/xml/log4cplus-1.2.0-rc5/lib
)

# set output bin directory
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

# set output lib directory
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

# make install
set(CMAKE_INSTALL_PREFIX ${PROJECT_BINARY_DIR}/../bin)
install(FILES bin/${target} DESTINATION .)


add_subdirectory(module)
add_subdirectory(center)
add_subdirectory(proxy)


# -------------useful function------------------ #
# set macro
# add_definitions("-DDEBUG")


# link dll
# target_link_libraries(comm pthread)

# set compiler option:CMAKE_C_FLAGS_DEBUG = c,CMAKE_CXX_FLAGS_DEBUG = cxx
# set(CMAKE_C_FLAGS_DEBUG "-g -Wall")
# set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall")
