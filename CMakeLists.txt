cmake_minimum_required(VERSION 2.8)
project(deanonymization)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -pthread")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

set(SOURCE_FILES
    src/data_analysis.cpp
    src/graph.cpp
    src/main.cpp
    src/match.cpp)

include_directories(.)

# BASELINE switch
option(BASELINE "Run Baseline Algorithm" OFF)
if(BASELINE)
    add_definitions(-DBASELINE)
endif(BASELINE)

# ALPHA-ROLESIM++ switch
option(ALPHA_R "Maintain only top-ALPHA-percent nodes for each node in a graph" ON)
if(ALPHA_R)
    add_definitions(-DALPHA_R)
endif(ALPHA_R)

add_executable(main ${SOURCE_FILES})
