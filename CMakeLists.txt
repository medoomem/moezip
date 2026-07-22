cmake_minimum_required(VERSION 3.16)
project(moezip CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
  add_compile_options(/EHsc /O2 /std:c++20)
else()
  add_compile_options(
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Release>:-march=native>
  )
endif()

# 1. Executable CLI
add_executable(moezip main.cpp vocab.cpp tokenizer.cpp ans.cpp router.cpp codec.cpp)
target_include_directories(moezip PRIVATE ${CMAKE_SOURCE_DIR})

# 2. Shared Library (DLL / .so)
add_library(moezip_dll SHARED api.cpp vocab.cpp tokenizer.cpp ans.cpp router.cpp codec.cpp)
target_include_directories(moezip_dll PRIVATE ${CMAKE_SOURCE_DIR})
set_target_properties(moezip_dll PROPERTIES OUTPUT_NAME "moezip")

# 3. Optional PyBind11 Python Module
find_package(pybind11 CONFIG QUIET)
if(pybind11_FOUND)
    pybind11_add_module(moezip_py bindings.cpp vocab.cpp tokenizer.cpp ans.cpp router.cpp codec.cpp)
    target_include_directories(moezip_py PRIVATE ${CMAKE_SOURCE_DIR})
    
    # Set platform-correct extension (.pyd for Windows, .so for Linux/macOS)
    if(WIN32)
        set_target_properties(moezip_py PROPERTIES OUTPUT_NAME "moezip" PREFIX "" SUFFIX ".pyd")
    else()
        set_target_properties(moezip_py PROPERTIES OUTPUT_NAME "moezip" PREFIX "" SUFFIX ".so")
    endif()
    
    message(STATUS "pybind11 found: 'moezip' python module target enabled.")
else()
    message(STATUS "pybind11 NOT found on system. Skipping python module target.")
endif()
