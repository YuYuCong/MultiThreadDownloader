cmake_minimum_required(VERSION 3.3)
project(windmill_downloader)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 14)

SET(BIN_DESTINATION ${PROJECT_SOURCE_DIR}/bin)

###################################### helpers ################################

######################################
# add_third_party()
################################
# CMake function to define a named third party library interface, with included
# headers and linked libraries.
#
# Parameters:
# NAME: name of target
# INCLUDES: list of include directories for the library
# LINKS: list of other libraries to be linked in to the library
# DEFINES: list of compile definitions
# COMPILE_OPTIONS: list of compile options
#
# Examples:
# add_third_party(
#   NAME
#     foo
#   INCLUDES
#     "${CMAKE_INSTALL_INCLUDEDIR}/foo"
#   LINKS
#     another
#   DEFINES
#     -Dbar
#   COMPILE_OPTIONS
#     -Wall
# )
function(ADD_THIRD_PARTY)
    cmake_parse_arguments(ADD_THIRD_PARTY
            ""
            "NAME"
            "INCLUDES;LINKS;DEFINES;COMPILE_OPTIONS"
            ${ARGN})

    unset(${ADD_THIRD_PARTY_NAME}_LIB_DEPENDS CACHE)
    add_library(${ADD_THIRD_PARTY_NAME} INTERFACE)
    target_include_directories(${ADD_THIRD_PARTY_NAME} INTERFACE
            ${ADD_THIRD_PARTY_INCLUDES})
    target_link_libraries(${ADD_THIRD_PARTY_NAME} INTERFACE
            ${ADD_THIRD_PARTY_LINKS})
    target_compile_definitions(${ADD_THIRD_PARTY_NAME} INTERFACE
            ${ADD_THIRD_PARTY_DEFINES})
    target_compile_options(${ADD_THIRD_PARTY_NAME} INTERFACE
            ${ADD_THIRD_PARTY_COMPILE_OPTIONS})
endfunction(ADD_THIRD_PARTY)

######################################
# add_to_all(target)
################################
#
# CMake macro to add build target to all. If target with suffix "_all" doesn't
# exist, one target with that name will be added.
macro(ADD_TO_ALL _target)
    if (NOT TARGET ${PROJECT_NAME}_all)
        add_custom_target(${PROJECT_NAME}_all)
    endif ()
    add_dependencies(${PROJECT_NAME}_all ${_target})
endmacro(ADD_TO_ALL)


######################################
# user_add_library()
################################
#
# CMake function to add a named library in build targets, with included headers,
# sources and linked libraries.
#
# Parameters:
# NAME: name of target
# INCLUDES: list of include directories for the library
# SRCS: list of source files for the library
# LINKS: list of other libraries to be linked in to the library
# DEFINES: list of compile definitions
# COMPILE_OPTIONS: list of compile options
#
# Example:
# user_add_library(
#   NAME
#     foo
#   INCLUDES
#     "${CMAKE_CURRENT_SOURCE_DIR}/include"
#   SRCS
#     "${CMAKE_CURRENT_SOURCE_DIR}/src/foo.cpp"
#   LINKS
#     another
#   DEFINES
#     -Dbar
#   COMPILE_OPTIONS
#     -Wall
# )
function(USER_ADD_LIBRARY)
    cmake_parse_arguments(ADD_LIBRARY
            ""
            "NAME"
            "INCLUDES;SRCS;LINKS;DEFINES;COMPILE_OPTIONS"
            ${ARGN})

    unset(${ADD_LIBRARY_NAME}_LIB_DEPENDS CACHE)
    if ("${ADD_LIBRARY_SRCS}" STREQUAL "")
        # library is an interface
        add_library(${ADD_LIBRARY_NAME} INTERFACE)
        target_include_directories(${ADD_LIBRARY_NAME} INTERFACE
                "$<BUILD_INTERFACE:${ADD_LIBRARY_INCLUDES}>")
        target_link_libraries(${ADD_LIBRARY_NAME} INTERFACE
                ${ADD_LIBRARY_LINKS})
        target_compile_definitions(${ADD_LIBRARY_NAME} INTERFACE
                ${ADD_LIBRARY_DEFINES})
        target_compile_options(${ADD_LIBRARY_NAME} INTERFACE
                ${ADD_LIBRARY_COMPILE_OPTIONS})
    else ("${ADD_LIBRARY_SRCS}" STREQUAL "")
        add_library(${ADD_LIBRARY_NAME} ${TRIFO_LIBRARY_TYPE} "")
        target_sources(${ADD_LIBRARY_NAME} PRIVATE ${ADD_LIBRARY_SRCS})
        target_include_directories(${ADD_LIBRARY_NAME} PUBLIC
                "$<BUILD_INTERFACE:${ADD_LIBRARY_INCLUDES}>")
        target_link_libraries(${ADD_LIBRARY_NAME} PRIVATE
                ${ADD_LIBRARY_LINKS})
        target_compile_definitions(${ADD_LIBRARY_NAME} PRIVATE
                ${ADD_LIBRARY_DEFINES})
        target_compile_options(${ADD_LIBRARY_NAME} PRIVATE
                ${WARNING_FLAGS}
                ${ADD_LIBRARY_COMPILE_OPTIONS})

        # Set output directory
        set_target_properties(${ADD_LIBRARY_NAME} PROPERTIES
                ARCHIVE_OUTPUT_DIRECTORY ${TRIFO_LIBRARY_OUTPUT_PATH}
                LIBRARY_OUTPUT_DIRECTORY ${TRIFO_LIBRARY_OUTPUT_PATH})
    endif ("${ADD_LIBRARY_SRCS}" STREQUAL "")

    # Add build target to all
    add_to_all(${ADD_LIBRARY_NAME})
endfunction(USER_ADD_LIBRARY)


###################################### project ################################

######################## libs ########################
### boost (filesystem)
find_package(Boost 1.65 REQUIRED COMPONENTS filesystem)
if (Boost_FOUND)
    MESSAGE(STATUS "Boost_INCLUDE_DIRS = ${Boost_INCLUDE_DIRS}")
    MESSAGE(STATUS "Boost_LIBRARIES = ${Boost_LIBRARIES}")
    MESSAGE(STATUS "Boost_LIB_VERSION = ${Boost_LIB_VERSION}")
    MESSAGE(STATUS "Boost_LIB_DIR = ${Boost_LIB_DIR}")

    include_directories(${Boost_INCLUDE_DIRS})
    add_third_party(
            NAME
            boost
            INCLUDES
            ${Boost_INCLUDE_DIRS}
            LINKS
            ${Boost_LIBRARIES}
    )
else ()
    MESSAGE(FATAL_ERROR "Boost not found")
endif ()


# Google Commandline Flags
find_package(gflags CONFIG HINTS ${CMAKE_INSTALL_PREFIX})
if (gflags_FOUND)
    add_third_party(NAME gflag LINKS gflags)
else (gflags_FOUND)
    MESSAGE(FATAL_ERROR "gflag not found")
endif ()

### Google Logging Library
find_package(glog CONFIG HINTS ${CMAKE_INSTALL_PREFIX} NO_CMAKE_PACKAGE_REGISTRY)
if (glog_FOUND)
    add_third_party(
            NAME glog
            LINKS glog::glog gflag
    )
else ()
    MESSAGE(FATAL_ERROR "glog not found")
endif ()

### thread_pool (user define lib)
user_add_library(NAME thread_pool
        INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/threadpool/include"
        SRCS "${CMAKE_CURRENT_SOURCE_DIR}/threadpool/src/threadpool.cc"
        LINKS
        boost
        glog
        gflag
        )

### downloader (user define lib)
user_add_library(NAME downloader
        INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/downloader/include"
        SRCS "${CMAKE_CURRENT_SOURCE_DIR}/downloader/src/downloader.cc"
        LINKS
        thread_pool
        curl
        boost
        glog
        gflag
        )


######################## target ########################

add_executable(windmill_downloader ${PROJECT_SOURCE_DIR}/app/windmill_downloader.cpp)
target_link_libraries(windmill_downloader
        thread_pool
        downloader
        boost
        glog
        gflag
        )
