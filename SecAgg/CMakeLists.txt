cmake_minimum_required (VERSION 3.0)
SET(CMAKE_C_COMPILER "gcc-9")
SET(CMAKE_CXX_COMPILER "g++-9")
# SET(CMAKE_BUILD_TYPE "Debug")
# SET(CMAKE_CXX_FLAGS "-g")
# SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")
# project (emp-sh2pc)
# set(NAME "emp-sh2pc")
project (emp-zk)
set(NAME "emp-zk")


find_path(CMAKE_FOLDER NAMES cmake/emp-tool-config.cmake)
include(${CMAKE_FOLDER}/cmake/common.cmake)
include(${CMAKE_FOLDER}/cmake/enable_rdseed.cmake)
include(${CMAKE_FOLDER}/cmake/enable_float.cmake)

set (CMAKE_PREFIX_PATH "../emp-ot/")
find_package(emp-ot REQUIRED)

include_directories(${EMP-OT_INCLUDE_DIRS})

# Installation
install(FILES cmake/emp-sh2pc-config.cmake DESTINATION cmake/)
install(DIRECTORY emp-sh2pc DESTINATION include/)

set(sources
  emp-zk/emp-zk-bool/emp-zk-bool.cpp
  emp-zk/emp-zk-arith/emp-zk-arith.cpp
  emp-zk/emp-zk-math/ZKmath-global.cpp    # our ZKmath
  emp-zk/emp-zk-math/ZKmath-functions.cpp
)
add_library(${NAME} SHARED ${sources})

install(FILES cmake/emp-zk-config.cmake DESTINATION cmake/)
install(DIRECTORY emp-zk DESTINATION include/)
install(TARGETS ${NAME} DESTINATION lib)

ENABLE_TESTING()
ADD_SUBDIRECTORY(test)
