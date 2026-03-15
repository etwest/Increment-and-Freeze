cmake_minimum_required(VERSION 3.18)

project(IAF LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Define a list variable with source files
set(IAF_SRCS
    bounded_iaf.cc
    projection.cc
)

# Add a library target using the list variable
add_library(IAF ${IAF_SRCS})
