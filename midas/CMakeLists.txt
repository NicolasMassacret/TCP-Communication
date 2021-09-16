#
# CMakeLists.txt for midas examples/experiment
#
# S.R. 28 May 2019
# K.O. 28 June 2019
#
# This cmakefile is dual use:
# if MIDAS_IN_TREE_BUILD is set, the example experiment is built as part of the midas compilation
# if MIDAS_IN_TREE_BUILD is unset, the example experiment is built against MIDAS installed at $MIDASSYS
#
# Optional settings for this cmakefile:
#
# $MIDASSYS - env.variable pointing to the location of MIDAS
# root-config in the $PATH - sets the location of ROOT for the analyzer
# -DNO_ROOT=1 - do not use ROOT
#
# Normal use:
#
# mkdir build
# cd build
# cmake ..
# or cmake3 ..
# make
#
# For testing the cmakefile inside the midas tree:
#
# cd $HOME/git/midas/examples/experiment
# mkdir build
# cd build
# MIDASSYS=$HOME/git/midas cmake ..
# make
#
cmake_minimum_required(VERSION 3.0)
project(vrae_frontends)
# check if we are called from parent directory
message(STATUS "MIDAS frontends: MIDASSYS set to " $ENV{MIDASSYS})
if (NOT EXISTS $ENV{MIDASSYS})
  message(FATAL_ERROR "Environment variable $MIDASSYS not defined, aborting.")
endif()
set(INC_PATH $ENV{MIDASSYS}/include $ENV{MIDASSYS}/mxml $ENV{MIDASSYS}/mvodb)
link_directories($ENV{MIDASSYS}/lib)
# default build type
if (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
    message(STATUS "Setting default build type to \"RelWithDebInfo\"")
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "" FORCE)
endif()
# enable certain compile warnings
add_compile_options(-Wall -Wformat=2 -Wno-format-nonliteral -Wno-strict-aliasing -Wuninitialized -Wno-unused-function)
# Optional ZLIB support
if (NO_ZLIB)
   message(STATUS "MIDAS: ZLIB support is disabled via NO_ZLIB")
else (NO_ZLIB)
find_package(ZLIB)
if (ZLIB_FOUND)
   message(STATUS "MIDAS: Found ZLIB version " ${ZLIB_VERSION_STRING})
   add_compile_options(-DHAVE_ZLIB)
   set(LIBS ${LIBS} -lz)
else ()
   message(STATUS "MIDAS: ZLIB not found")
endif()
endif (NO_ZLIB)
# ass required linux libraries
if (${CMAKE_SYSTEM_NAME} MATCHES Linux)
   set(LIBS ${LIBS} -lpthread -lutil -lrt)
endif()
add_definitions(-DOS_LINUX)
add_library(mfe $ENV{MIDASSYS}/lib/mfe.o)
set_target_properties(mfe PROPERTIES LINKER_LANGUAGE CXX)

add_library(KO KOtcp.cxx)
add_executable(LabViewDriver LabViewDriver.cxx)
target_include_directories(KO PRIVATE ${INC_PATH})
target_include_directories(LabViewDriver PRIVATE ${INC_PATH})
target_link_libraries(LabViewDriver mfe midas KO ${LIBS})

# first we can indicate the documentation build as an option and set it to ON by default
option(BUILD_DOC "Build documentation" ON)
# check if Doxygen is installed
find_package(Doxygen)
if (DOXYGEN_FOUND)
    # set input and output files
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    # request to configure the file
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    message("Doxygen build started")
    # note the option ALL which allows to build the docs together with the application
    add_custom_target( doc_doxygen ALL
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM )
else (DOXYGEN_FOUND)
  message("Doxygen need to be installed to generate the doxygen documentation")
endif (DOXYGEN_FOUND)
