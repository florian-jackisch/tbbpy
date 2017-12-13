cmake_minimum_required(VERSION 3.5.1)

project(tbb-python CXX)

# Set default compiler flags
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(WARNING_FLAGS "-Wall" "-Wextra")

# Create compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Place all executables in bin, libraries in lib
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Download dependencies
include(cmake/DownloadDependencies.cmake)
find_package(Threads REQUIRED)

# Create the tbb-python-example executable
add_executable(tbb-python-example "examples/tbb-python.cpp")
target_compile_options(tbb-python-example PRIVATE ${WARNING_FLAGS})
target_link_libraries(tbb-python-example
    PRIVATE TBB::tbb
    PRIVATE pybind11::embed
    PRIVATE Threads::Threads
)