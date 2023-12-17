cmake_minimum_required (VERSION 3.14)
project ("lightnet" LANGUAGES C CXX)
set (CMAKE_CXX_STANDARD 17)

# option() honor variables: https://cmake.org/cmake/help/latest/policy/CMP0077.html
# A project that embeds another project as a subdirectory may want to hard-code options
# of the subproject to build the way it needs.
if (POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)
endif (POLICY CMP0077)

set(MY_CXX_FLAGS -Wall -fPIC -g)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# CMake Option:
# https://cmake.org/cmake/help/book/mastering-cmake/chapter/CMake%20Cache.html
# https://stackoverflow.com/questions/8709877/cmake-string-options

# Command line options: -DLNET_CPPNS=lightnet -DLNET_POLLER=epoll/poll/select -DLNET_DEBUG=ON
# You can set the C++ namespace as your need.
set(LNET_CPPNS "" CACHE STRING "Custom the library namespace.")
set(LNET_POLLER "" CACHE STRING "Choose polling system, valid values are epoll, poll or select [default=autodetect]")
option(LNET_BUILD_DNS "Build dns sub-module" ON)
option(LNET_DEBUG "Print debug message to stdout/stderr" OFF)
option(LNET_BUILD_TESTS "Build tests and demos" OFF)

# This is a cmake global variable.
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

if(LNET_CPPNS STREQUAL "")
  set(LNETNS "${PROJECT_NAME}")
else()
  set(LNETNS "${LNET_CPPNS}")
endif()
message(STATUS "Using namepsace '${LNETNS}'")

include(FetchContent) # New in version 3.11.

if(LNET_BUILD_DNS)
  set(LIB_ARES cares)

  FetchContent_Declare(${LIB_ARES}
    GIT_REPOSITORY  https://github.com/c-ares/c-ares.git
    GIT_TAG         cares-1_23_0
  )

  if(NOT ${LIB_ARES}_POPULATED)
    FetchContent_Populate(${LIB_ARES})

    # Override build options of c-ares.

    # Store the old value
    set(CARES_BUILD_TESTS_OLD ${CARES_BUILD_TESTS})
    set(CARES_BUILD_CONTAINER_TESTS_OLD ${CARES_BUILD_CONTAINER_TESTS})
    set(CARES_BUILD_TOOLS_OLD ${CARES_BUILD_TOOLS})

    # Set new value
    set(CARES_BUILD_TESTS OFF CACHE INTERNAL "Override option CARES_BUILD_TESTS")
    set(CARES_BUILD_CONTAINER_TESTS OFF CACHE INTERNAL "Override option CARES_BUILD_CONTAINER_TESTS")
    set(CARES_BUILD_TOOLS OFF CACHE INTERNAL "Override option CARES_BUILD_TOOLS")
    if(BUILD_SHARED_LIBS)
      set(CARES_STATIC OFF CACHE INTERNAL "Override option CARES_STATIC")
      set(CARES_SHARED ON CACHE INTERNAL "Override option CARES_SHARED")
    else()
      set(CARES_STATIC ON CACHE INTERNAL "Override option CARES_STATIC")
      set(CARES_SHARED OFF CACHE INTERNAL "Override option CARES_SHARED")
    endif()

    add_subdirectory(${${LIB_ARES}_SOURCE_DIR} ${${LIB_ARES}_BINARY_DIR})

    # Restore the old value
    set(CARES_BUILD_TESTS ${CARES_BUILD_TESTS_OLD} CACHE BOOL "Restore CARES_BUILD_TESTS" FORCE)
    set(CARES_BUILD_CONTAINER_TESTS ${CARES_BUILD_CONTAINER_TESTS_OLD} CACHE BOOL "Restore CARES_BUILD_CONTAINER_TESTS" FORCE)
    set(CARES_BUILD_TOOLS ${CARES_BUILD_TOOLS_OLD} CACHE BOOL "Restore CARES_BUILD_TOOLS" FORCE)
  endif()
endif()

if(LNET_BUILD_TESTS)
  message(STATUS "Build tests and demos")

  set(GTEST googletest)
  set(GFLAGS gflags)
  FetchContent_Declare(${GTEST}
    GIT_REPOSITORY    https://github.com/google/googletest
    GIT_TAG           v1.12.0
  )
  FetchContent_Declare(${GFLAGS}
    GIT_REPOSITORY  https://github.com/gflags/gflags.git
    GIT_TAG         v2.2.2
  )

  if(NOT ${GTEST}_POPULATED)
    FetchContent_Populate(${GTEST})

    # Store the old value
    set(BUILD_GMOCK_OLD ${BUILD_GMOCK})
    set(INSTALL_GTEST_OLD ${INSTALL_GTEST})

    # Set new value
    set(BUILD_GMOCK OFF CACHE INTERNAL "Override option BUILD_GMOCK")
    set(INSTALL_GTEST OFF CACHE INTERNAL "Override option INSTALL_GTEST")

    add_subdirectory(${${GTEST}_SOURCE_DIR} ${${GTEST}_BINARY_DIR})

    # Restore the old value
    set(BUILD_GMOCK ${BUILD_GMOCK_OLD} CACHE BOOL "Restore BUILD_GMOCK" FORCE)
    set(INSTALL_GTEST ${INSTALL_GTEST_OLD} CACHE BOOL "Restore INSTALL_GTEST" FORCE)
  endif()

  if(NOT ${GFLAGS}_POPULATED)
    FetchContent_Populate(${GFLAGS})

    if(BUILD_SHARED_LIBS)
      set(GFLAGS_BUILD_STATIC_LIBS OFF CACHE INTERNAL "Override option GFLAGS_BUILD_STATIC_LIBS")
      set(GFLAGS_BUILD_SHARED_LIBS ON CACHE INTERNAL "Override option GFLAGS_BUILD_SHARED_LIBS")
    else()
      set(GFLAGS_BUILD_STATIC_LIBS ON CACHE INTERNAL "Override option GFLAGS_BUILD_STATIC_LIBS")
      set(GFLAGS_BUILD_SHARED_LIBS OFF CACHE INTERNAL "Override option GFLAGS_BUILD_SHARED_LIBS")
    endif()

    add_subdirectory(${${GFLAGS}_SOURCE_DIR} ${${GFLAGS}_BINARY_DIR})
  endif()
endif()

if(LNET_BUILD_TESTS OR LNET_DEBUG)
  set(LIB_FMT fmt)
  FetchContent_Declare(${LIB_FMT}
    GIT_REPOSITORY  https://github.com/fmtlib/fmt.git
    GIT_TAG         10.1.1
  )
  if(NOT ${LIB_FMT}_POPULATED)
    FetchContent_Populate(${LIB_FMT})
    add_subdirectory(${${LIB_FMT}_SOURCE_DIR} ${${LIB_FMT}_BINARY_DIR})
  endif()
endif()

# Refer to https://github.com/zeromq/libzmq/blob/master/CMakeLists.txt
include(CheckCSourceRuns) # check_c_source_runs
include(CheckCSourceCompiles) # check_c_source_compiles
include(CheckCXXSymbolExists) # check_cxx_symbol_exists

list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULES_DIR ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND CMAKE_MODULE_PATH ${CMAKE_MODULES_DIR})
include(SourceRunChecks)

if(LNET_POLLER STREQUAL "")
  check_cxx_symbol_exists(epoll_create sys/epoll.h HAVE_EPOLL)
  if(HAVE_EPOLL)
    set(LNET_POLLER "epoll")
    check_cxx_symbol_exists(epoll_create1 sys/epoll.h HAVE_EPOLL_CLOEXEC)
    if(HAVE_EPOLL_CLOEXEC)
      set(POLLER_USE_EPOLL_CLOEXEC 1)
    endif()
  endif()
endif()

if(LNET_POLLER STREQUAL "")
  check_cxx_symbol_exists(poll poll.h HAVE_POLL)
  if(HAVE_POLL)
    set(LNET_POLLER "poll")
  endif()
endif()

if(LNET_POLLER STREQUAL "")
  check_cxx_symbol_exists(select sys/select.h HAVE_SELECT)
  if(HAVE_SELECT)
    set(LNET_POLLER "select")
  else()
    message(FATAL_ERROR "Could not autodetect polling method")
  endif()
endif()

if(LNET_POLLER STREQUAL "epoll" OR LNET_POLLER STREQUAL "poll" OR LNET_POLLER STREQUAL "select")
  message(STATUS "Using polling method: ${LNET_POLLER}")
  string(TOUPPER ${LNET_POLLER} LNET_UPPER_POLLER)
  set(POLLER_USE_${LNET_UPPER_POLLER} 1)
else()
  message(FATAL_ERROR "Invalid polling method")
endif()

check_cxx_symbol_exists(accept4 sys/socket.h HAVE_ACCEPT4)

# Execution checks

check_sock_cloexec()
check_o_cloexec()
check_so_keepalive()
check_tcp_keepcnt()
check_tcp_keepidle()
check_tcp_keepintvl()
check_tcp_keepalive()
check_so_priority()

# Compilation checks
check_pthread_setname()
check_pthread_setaffinity()
check_noexcept()

# Delete any config.h left by configure
file(REMOVE ${CMAKE_CURRENT_SOURCE_DIR}/config.h)
configure_file(config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/config.h @ONLY)

add_subdirectory(event)
add_subdirectory(address)
if(LNET_BUILD_DNS)
  add_subdirectory(dns)
endif()
