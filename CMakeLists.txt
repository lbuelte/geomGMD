cmake_minimum_required(VERSION 3.16)
project(geomGMD LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release")
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(GEOMGMD_BUILD_CLUSTERING "Build the clustering library and PolygonClustering (requires GDAL)" ON)

find_package(GUROBI REQUIRED)
if(GEOMGMD_BUILD_CLUSTERING)
  find_package(GDAL)
  if(NOT GDAL_FOUND)
    message(FATAL_ERROR "GDAL not found. GDAL is only needed for the clustering part; "
                        "to build geomGMD without it, configure with -DGEOMGMD_BUILD_CLUSTERING=OFF.")
  endif()
endif()

# The code is organised in layers, each a separate library that only depends on the layers below:
#   utility    (geometric graphs, angles, edge maps, logging)
#   distance   (geomGMD solver; Gurobi)
#   io         (configuration, command line, output files)
#   clustering (PAM, CLARA, CLARANS, GeoPackage output; GDAL; optional)

# Utility: geometric graphs, angle computation, edge map type, logging
file(GLOB UTILITY_SOURCES CONFIGURE_DEPENDS src/utility/*.cpp)
add_library(geomGMD_utility ${UTILITY_SOURCES})
target_include_directories(geomGMD_utility PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/external
)

# Distance: computation of geomGMD with Gurobi
file(GLOB DISTANCE_SOURCES CONFIGURE_DEPENDS src/distance/*.cpp)
add_library(geomGMD_distance ${DISTANCE_SOURCES})
target_link_libraries(geomGMD_distance PUBLIC
    geomGMD_utility
    GUROBI::GUROBI
)

# IO: configuration files, command line, output files
file(GLOB IO_SOURCES CONFIGURE_DEPENDS src/io/*.cpp)
add_library(geomGMD_io ${IO_SOURCES})
target_link_libraries(geomGMD_io PUBLIC geomGMD_distance)
target_include_directories(geomGMD_io PRIVATE ${PROJECT_SOURCE_DIR}/external/tomlplusplus/include)

# Executables for single distance computations, full dataset computations (distance matrix)
# and plotting of computed edge maps
add_executable(geomGMD_SingleDistanceComputation apps/main_geomGMDsingledistance.cpp)
target_link_libraries(geomGMD_SingleDistanceComputation PRIVATE geomGMD_io)

add_executable(geomGMD_FullDatasetComputations apps/main_geomGMDcomputations.cpp)
target_link_libraries(geomGMD_FullDatasetComputations PRIVATE geomGMD_io)

add_executable(PlotEdgemaps apps/main_plotEdgemaps.cpp)
target_link_libraries(PlotEdgemaps PRIVATE geomGMD_io)

# Clustering (optional): PAM, CLARA, CLARANS and the clustering application
if(GEOMGMD_BUILD_CLUSTERING)
  file(GLOB CLUSTERING_SOURCES CONFIGURE_DEPENDS src/clustering/*.cpp)
  add_library(geomGMD_clustering ${CLUSTERING_SOURCES})
  target_link_libraries(geomGMD_clustering PUBLIC
      geomGMD_io
      GDAL::GDAL
  )

  add_executable(PolygonClustering apps/main_polygonclustering.cpp)
  target_link_libraries(PolygonClustering PRIVATE geomGMD_clustering)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  include(FeatureSummary)
  feature_summary(WHAT ALL)
endif()
