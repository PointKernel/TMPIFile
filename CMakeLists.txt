cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(TMPIFile CXX)

##### PATH OF CMAKE PACKAGES
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# MPI
find_package(MPI REQUIRED)
# ROOT
find_package(ROOT REQUIRED)

# No RPATH build
set(CMAKE_SKIP_BUILD_RPATH  TRUE)

# Set the install path if not set by -DCMAKE_INSTALL_PREFIX=...
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR} CACHE PATH "Install path" FORCE )
  message(STATUS "Default installation path is : ${CMAKE_INSTALL_PREFIX}")
else(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  message(STATUS "Installation path is : ${CMAKE_INSTALL_PREFIX}")
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

# Compilation options
message(STATUS "C++ compiler is : ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ compiler identification is : ${CMAKE_CXX_COMPILER_ID}")

set(CMAKE_CXX_FLAGS "-Wshadow -Wall -W -Woverloaded-virtual -fsigned-char -Wextra -std=c++14 -g")

add_subdirectory(src)
add_subdirectory(test)

# Build environment script
configure_file(env_tmpi.sh.in ${CMAKE_CURRENT_BINARY_DIR}/env_tmpi.sh @ONLY IMMEDIATE)
# and install it
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/env_tmpi.sh
	DESTINATION ${CMAKE_INSTALL_PREFIX})
