# CMakeLists.txt for ics center 


# set include directories
include_directories(
#	${PROJECT_SOURCE_DIR}/src
)

# set lib directories
link_directories(
#	/usr/local/gcc-5.2/lib64
#	/home/xml/log4cplus-1.2.0-rc5/lib
)

# target name
set(target "icsmodule")


# source file
aux_source_directory(. SRC_FILES)


# exec
add_library(${target} STATIC ${SRC_FILES})


# link dll
target_link_libraries(${target} pthread odbc log4cplus rt)
