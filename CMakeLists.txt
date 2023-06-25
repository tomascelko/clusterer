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

set(onnx_lib_dir ${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-linux-x64-1.14.1/lib/)
set(onnx_include_path ${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-linux-x64-1.14.1/include/)
find_library(onnx_lib_path onnxruntime
    PATHS ${onnx_lib_dir})

AUX_SOURCE_DIRECTORY(./src SOURCES)
file(GLOB_RECURSE LIB_SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.cpp
)
#set(SOURCES ./src/main.cpp)


add_library(lcclusterer SHARED ${LIB_SRC_FILES})

set_target_properties(lcclusterer PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(lcclusterer PROPERTIES LIBRARY_OUTPUT_DIRECTORY "./lib")
#set_target_properties(lcclusterer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./bin")
target_include_directories(lcclusterer PUBLIC ${onnx_include_path})
target_link_libraries(lcclusterer PUBLIC ${onnx_lib_path})


#target_link_libraries(lcclusterer ${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-linux-x64-1.14.1/lib/libonnxruntime.so)

add_executable(cclusterer ${SOURCES})

set_target_properties(cclusterer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./bin")
target_include_directories(cclusterer PUBLIC ${onnx_include_path})
target_link_libraries(cclusterer ${onnx_lib_path})
#target_link_libraries(cclusterer ${Boost_LIBRARIES} stdc++fs) #Python3::Python Python3::NumPy)


