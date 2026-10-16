cmake_minimum_required(VERSION 3.16)
project(geomGMD LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release")
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(GDAL REQUIRED)
find_package(GUROBI REQUIRED)

# Collect all .cpp files in src/ and all subdirectories
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS src/*.cpp)

add_library(geomGMD_lib 
    ${SRC_FILES}
)

target_include_directories(geomGMD_lib PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/external
    ${PROJECT_SOURCE_DIR}/external/tomlplusplus/include
)

target_link_libraries(geomGMD_lib PUBLIC
    GUROBI::GUROBI
    GDAL::GDAL
)

# Executables for single distance computations, full dataset computations (distance matrix),
# clustering and plotting of computed edge maps
add_executable(geomGMD_SingleDistanceComputation apps/main_geomGMDsingledistance.cpp)
target_link_libraries(geomGMD_SingleDistanceComputation PRIVATE geomGMD_lib)

add_executable(geomGMD_FullDatasetComputations apps/main_geomGMDcomputations.cpp)
target_link_libraries(geomGMD_FullDatasetComputations PRIVATE geomGMD_lib)

add_executable(PolygonClustering apps/main_polygonclustering.cpp)
target_link_libraries(PolygonClustering PRIVATE geomGMD_lib)

add_executable(PlotNodemaps apps/main_plotNodemaps.cpp)
target_link_libraries(PlotNodemaps PRIVATE geomGMD_lib)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  include(FeatureSummary)
  feature_summary(WHAT ALL)
endif()
