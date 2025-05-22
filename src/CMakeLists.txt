set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(caep)

target_include_directories(caep PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/caddies
    ${CMAKE_CURRENT_SOURCE_DIR}/caddies/cv
    ${CMAKE_CURRENT_SOURCE_DIR}/caddies/ext
    ${CMAKE_CURRENT_SOURCE_DIR}/caddies/file
    ${CMAKE_CURRENT_SOURCE_DIR}/caddies/json
    ${CMAKE_CURRENT_SOURCE_DIR}/caddies/log
    ${CMAKE_CURRENT_SOURCE_DIR}/caddies/memory
    ${CMAKE_CURRENT_SOURCE_DIR}/caddies/perf
    ${CMAKE_CURRENT_SOURCE_DIR}/caddies/regex
    ${CMAKE_CURRENT_SOURCE_DIR}/caddies/threadpool
    ${CMAKE_CURRENT_SOURCE_DIR}/caddies/util
    ${CMAKE_CURRENT_SOURCE_DIR}/gpu_helper
    ${CMAKE_CURRENT_SOURCE_DIR}/gtest
    ${CMAKE_CURRENT_SOURCE_DIR}/gtest/include
    ${CMAKE_CURRENT_SOURCE_DIR}/gtest/src
    ${CMAKE_CURRENT_SOURCE_DIR}/caep/inc
)

file(GLOB_RECURSE sources 
    # ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/caddies/**/*.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/caddies/**/*.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/gpu_helper/*.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/gtest/src/*.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/caep/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/run/*.cpp
)

target_sources(caep PRIVATE ${sources})

# version
# 检查版本变量是否定义
if(NOT DEFINED CAEP_VERSION_MAJOR OR 
   NOT DEFINED CAEP_VERSION_MINOR OR 
   NOT DEFINED CAEP_VERSION_REVISION OR 
   NOT DEFINED CAEP_VERSION)
    message(FATAL_ERROR "Version variables are not defined. Please set CAEP_VERSION_MAJOR, CAEP_VERSION_MINOR, CAEP_VERSION_REVISION, and CAEP_VERSION.")
endif()

target_compile_definitions(caep PRIVATE 
    CAEP_VERSION_MAJOR=${CAEP_VERSION_MAJOR}
    CAEP_VERSION_MINOR=${CAEP_VERSION_MINOR}
    CAEP_VERSION_REVISION=${CAEP_VERSION_REVISION}
    CAEP_VERSION="${CAEP_VERSION}"
)
