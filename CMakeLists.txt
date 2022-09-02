cmake_minimum_required(VERSION 3.10)

# set the project name
project(cclusterer)

# add the executable
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "-pthread")
set(CMAKE_BUILD_TYPE "Release")
AUX_SOURCE_DIRECTORY(./src SOURCES)
#set(SOURCES ./src/main.cpp)
add_executable(cclusterer ${SOURCES})
set_target_properties(cclusterer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./bin")
