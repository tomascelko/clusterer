cmake_minimum_required(VERSION 2.8)

# set the project name
project(cclusterer)

# add the executable
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "-pthread -lstdc++fs")
set(CMAKE_BUILD_TYPE "Release")

set(Boost_USE_STATIC_LIBS OFF) 
set(Boost_USE_MULTITHREADED ON)  
set(Boost_USE_STATIC_RUNTIME OFF) 
find_package(Boost 1.71.0 COMPONENTS iostreams) 
find_library(STDCPPFS_LIBRARY NAMES stdc++fs)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS}) 
endif()
#find_package(Python3 COMPONENTS Development NumPy) 
#include_directories(${Python2_INCLUDE_DIRS} ${Python2_NumPy_INCLUDE_DIRS})
AUX_SOURCE_DIRECTORY(./src SOURCES)

#set(SOURCES ./src/main.cpp)


add_executable(cclusterer ${SOURCES})
#add_library(lcclusterer STATIC ${SOURCES})


#set_target_properties(lcclusterer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./lib")
set_target_properties(cclusterer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./bin")
target_include_directories(cclusterer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-linux-x64-1.14.1/include/)

#target_link_directories(cclusterer PUBLIC "./onnxruntime-linux-x64-1.14.1/lib/")

target_link_libraries(cclusterer ${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-linux-x64-1.14.1/lib/libonnxruntime.so)

#target_link_libraries(cclusterer ${Boost_LIBRARIES} stdc++fs) #Python3::Python Python3::NumPy)


