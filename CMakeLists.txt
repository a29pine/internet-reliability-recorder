cmake_minimum_required(VERSION 3.12)
project(irr VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

enable_testing()
option(IRR_ENABLE_PCAP "Enable passive capture features" OFF)

enable_testing()
add_subdirectory(src)
add_subdirectory(tests)
