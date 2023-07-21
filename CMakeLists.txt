cmake_minimum_required(VERSION 2.8)

# set the project name
project(clusterer)

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

set(onnx_lib_dirs ${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-linux-x64-1.14.1/lib/;${CMAKE_CURRENT_SOURCE_DIR}/build/packages/Microsoft.ML.OnnxRuntime.1.15.1/runtimes/win-x64/native/;${ONNX_LIBRARY_PATH})
set(onnx_include_path ${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-linux-x64-1.14.1/include/)
find_library(onnx_lib_path onnxruntime
    PATHS ${onnx_lib_dirs})
if(NOT onnx_lib_path)
	message(FATAL_ERROR "onnx lib was not found")
endif()
AUX_SOURCE_DIRECTORY(./src SOURCES)
file(GLOB_RECURSE LIB_SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.cpp
)
#set(SOURCES ./src/main.cpp)


add_library(lclusterer SHARED ${LIB_SRC_FILES})

set_target_properties(lclusterer PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(lclusterer PROPERTIES LIBRARY_OUTPUT_DIRECTORY "./lib")
set_target_properties(lclusterer PROPERTIES LIBRARY_OUTPUT_DIRECTORY_RELEASE "./lib")
set_target_properties(lclusterer PROPERTIES LIBRARY_OUTPUT_DIRECTORY_DEBUG "./lib")

#set_target_properties(lclusterer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./bin")
target_include_directories(lclusterer PUBLIC ${onnx_include_path})
target_link_libraries(lclusterer PUBLIC ${onnx_lib_path})


#target_link_libraries(lclusterer ${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-linux-x64-1.14.1/lib/libonnxruntime.so)



 
add_executable(clusterer ${SOURCES})

set_target_properties(clusterer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./bin")
set_target_properties(clusterer PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG "./bin")
set_target_properties(clusterer PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "./bin")

target_include_directories(clusterer PUBLIC ${onnx_include_path})
target_link_libraries(clusterer ${onnx_lib_path})

#copy the dll to the binary if it was found
find_file(onnx_dll_path "onnxruntime.dll" 
	PATHS onnx_lib_dirs) 
get_filename_component( onnx_lib_dir ${onnx_lib_path} DIRECTORY)

#message(FATAL_ERROR ${onnx_lib_dir})
file(GLOB FILES_TO_COPY ${onnx_lib_dir}/*.dll)

foreach(FILE ${FILES_TO_COPY})
    get_filename_component(FILE_NAME ${FILE} NAME)
    add_custom_command(TARGET clusterer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${FILE}"
        "./bin")
endforeach()
