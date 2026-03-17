cmake_minimum_required(VERSION 3.18)

project(IAF LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# Define a list variable with source files
set(IAF_SRCS
    src/bounded_iaf.cc
    src/projection.cc
    src/increment_and_freeze.cc
)

# Add a library target using the list variable
add_library(IAF ${IAF_SRCS})
target_include_directories(IAF PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/includes>
    $<INSTALL_INTERFACE:includes> 
)



add_executable(simulation src/simulation.cc src/ost_cache_sim.cc src/ostree.cc)  
target_include_directories(simulation PRIVATE container)
target_link_libraries(simulation PRIVATE IAF)   
