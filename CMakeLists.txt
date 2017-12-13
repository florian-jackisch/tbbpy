cmake_minimum_required(VERSION 3.5.1)

project(tbb-python CXX)

# Create compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Place all executables in bin, libraries in lib
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Download dependencies
include(cmake/DownloadDependencies.cmake)

