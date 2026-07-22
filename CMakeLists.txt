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

# Executable CLI
add_executable(moezip main.cpp vocab.cpp tokenizer.cpp ans.cpp router.cpp codec.cpp)
target_include_directories(moezip PRIVATE ${CMAKE_SOURCE_DIR})

# Shared Library (DLL)
add_library(moezip_dll SHARED api.cpp vocab.cpp tokenizer.cpp ans.cpp router.cpp codec.cpp)
target_include_directories(moezip_dll PRIVATE ${CMAKE_SOURCE_DIR})
set_target_properties(moezip_dll PROPERTIES OUTPUT_NAME "moezip")
