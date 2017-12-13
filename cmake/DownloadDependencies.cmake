# Download dependencies
include("${CMAKE_SOURCE_DIR}/cmake/DownloadProject.cmake")

# TBB
download_project(
    PROJ tbb
    GIT_REPOSITORY https://github.com/01org/tbb.git
    GIT_TAG tbb_2018
    UPDATE_DISCONNECTED 1
)
include("${tbb_SOURCE_DIR}/cmake/TBBBuild.cmake")
tbb_build(TBB_ROOT ${tbb_SOURCE_DIR} CONFIG_DIR TBB_DIR MAKE_ARGS stdver=c++14)
find_package(TBB REQUIRED tbb)

# pybind11
download_project(
    PROJ pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG master
    UPDATE_DISCONNECTED 1
)
add_subdirectory("${pybind11_SOURCE_DIR}")