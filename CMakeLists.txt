cmake_minimum_required(VERSION 3.10)

# set the project name
project(cclusterer)

# add the executable
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "-pthread")
set(CMAKE_BUILD_TYPE "Release")

set(Boost_USE_STATIC_LIBS OFF) 
set(Boost_USE_MULTITHREADED ON)  
set(Boost_USE_STATIC_RUNTIME OFF) 
find_package(Boost 1.71.0 COMPONENTS iostreams) 

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS}) 

    AUX_SOURCE_DIRECTORY(./src SOURCES)
#set(SOURCES ./src/main.cpp)
    add_executable(cclusterer ${SOURCES})
    set_target_properties(cclusterer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./bin")
    target_link_libraries(cclusterer ${Boost_LIBRARIES})

endif()
