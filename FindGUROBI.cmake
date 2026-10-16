# Finds the Gurobi Optimizer (C and C++ interface).
#
# The installation directory is taken from GUROBI_DIR (CMake or environment variable)
# or from the environment variable GUROBI_HOME, e.g. /opt/gurobi1300/linux64.
#
# Defines the imported target GUROBI::GUROBI and the variables
#   GUROBI_FOUND, GUROBI_INCLUDE_DIRS, GUROBI_LIBRARY, GUROBI_CXX_LIBRARY, GUROBI_CXX_DEBUG_LIBRARY

set(GUROBI_DIR "$ENV{GUROBI_DIR}" CACHE PATH "Path to the Gurobi installation directory")

set(_GUROBI_HINTS ${GUROBI_DIR} $ENV{GUROBI_HOME})

find_path(
    GUROBI_INCLUDE_DIRS
    NAMES gurobi_c.h
    HINTS ${_GUROBI_HINTS}
    PATH_SUFFIXES include)

find_library(
    GUROBI_LIBRARY
    NAMES gurobi gurobi130 gurobi120 gurobi110 gurobi100
    HINTS ${_GUROBI_HINTS}
    PATH_SUFFIXES lib)

if(MSVC)
    if(MSVC_TOOLSET_VERSION EQUAL 143)
        set(_GUROBI_MSVC_YEAR "2022")
    elseif(MSVC_TOOLSET_VERSION EQUAL 142)
        set(_GUROBI_MSVC_YEAR "2019")
    elseif(MSVC_TOOLSET_VERSION EQUAL 141)
        set(_GUROBI_MSVC_YEAR "2017")
    elseif(MSVC_TOOLSET_VERSION EQUAL 140)
        set(_GUROBI_MSVC_YEAR "2015")
    endif()

    find_library(
        GUROBI_CXX_LIBRARY
        NAMES gurobi_c++md${_GUROBI_MSVC_YEAR}
        HINTS ${_GUROBI_HINTS}
        PATH_SUFFIXES lib)
    find_library(
        GUROBI_CXX_DEBUG_LIBRARY
        NAMES gurobi_c++mdd${_GUROBI_MSVC_YEAR}
        HINTS ${_GUROBI_HINTS}
        PATH_SUFFIXES lib)
else()
    find_library(
        GUROBI_CXX_LIBRARY
        NAMES gurobi_c++
        HINTS ${_GUROBI_HINTS}
        PATH_SUFFIXES lib)
    set(GUROBI_CXX_DEBUG_LIBRARY ${GUROBI_CXX_LIBRARY})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GUROBI
    REQUIRED_VARS GUROBI_LIBRARY GUROBI_CXX_LIBRARY GUROBI_INCLUDE_DIRS)

if(GUROBI_FOUND AND NOT TARGET GUROBI::GUROBI)
    add_library(GUROBI::GUROBI INTERFACE IMPORTED)
    set_target_properties(GUROBI::GUROBI PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${GUROBI_INCLUDE_DIRS}")
    # The C++ interface has to be linked before the C library.
    target_link_libraries(GUROBI::GUROBI INTERFACE
        optimized ${GUROBI_CXX_LIBRARY}
        debug ${GUROBI_CXX_DEBUG_LIBRARY}
        ${GUROBI_LIBRARY})
endif()

mark_as_advanced(GUROBI_INCLUDE_DIRS GUROBI_LIBRARY GUROBI_CXX_LIBRARY GUROBI_CXX_DEBUG_LIBRARY)
