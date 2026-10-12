cmake_minimum_required(VERSION 3.5)
project(geomGMD)

option(CXX "enable C++ compilation" ON)
if(CXX)
  enable_language(CXX)
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}")
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Visual Studio compiler with static runtime libraries
if(MSVC AND MT)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# ignore BOOST deprecated headers
add_definitions("-DBOOST_ALLOW_DEPRECATED_HEADERS")
add_definitions("-DBOOST_BIND_GLOBAL_PLACEHOLDERS")

find_package(GDAL REQUIRED)
if(GDAL_FOUND)
  message(STATUS "Found GDAL version ${GDAL_VERSION}")
else()
  message(FATAL_ERROR "GDAL not found")
endif()

find_package(GUROBI REQUIRED)
message(STATUS "GUROBI include dirs: ${GUROBI_INCLUDE_DIRS}")
include_directories(${GUROBI_INCLUDE_DIRS})

# include_directories(${GUROBI_HOME}/include)
link_directories(${GUROBI_LIBRARY})

# Collect all .cpp files in src/ and all subdirectories
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS src/*.cpp)
list(FILTER SRC_FILES EXCLUDE REGEX ".*/main[^/]*\\.cpp$")

add_library(geomGMD_lib ${SRC_FILES})

# Inlude paths for private directories
target_include_directories(geomGMD_lib PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${GUROBI_INCLUDE_DIRS}
    ${GDAL_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/external/tomlplusplus/include
    ${PROJECT_SOURCE_DIR}/external
)

if(CXX)
  target_link_libraries(geomGMD_lib PUBLIC
          optimized ${GUROBI_CXX_LIBRARY}
          debug ${GUROBI_CXX_DEBUG_LIBRARY}
  )
endif()

target_link_libraries(geomGMD_lib PUBLIC
        ${GUROBI_LIBRARY}
        ${GDAL_LIBRARIES}
)

# create independent executables for distance computation, clustering and for the comparison of computed clusterings
add_executable(PolygonClustering src/main_polygonclustering.cpp)
target_link_libraries(PolygonClustering PRIVATE geomGMD_lib)

add_executable(geomGMD_FullDatasetComputations src/main_geomGMDcomputations.cpp)
target_link_libraries(geomGMD_FullDatasetComputations PRIVATE geomGMD_lib)

add_executable(PlotNodemaps src/main_plotNodemaps.cpp)
target_link_libraries(PlotNodemaps PRIVATE geomGMD_lib)

add_executable(geomGMD_SingleDistanceComputation src/main_geomGMDsingledistance.cpp)
target_link_libraries(geomGMD_SingleDistanceComputation PRIVATE geomGMD_lib)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
  include(FeatureSummary)
  feature_summary(WHAT ALL)
endif()
