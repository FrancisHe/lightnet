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
# https://cmake.org/cmake/help/latest/command/option.html
# https://stackoverflow.com/questions/8709877/cmake-string-options

# You can set the C++ namespace on your need.
set(LNET_CPPNS "" CACHE STRING "Custom the library namespace.")
if(LNET_CPPNS STREQUAL "")
  set(LNETNS "${PROJECT_NAME}")
else()
  set(LNETNS "${LNET_CPPNS}")
endif()
message(STATUS "Using namepsace '${LNETNS}'")

# If a variable is in the cache, it can still be overridden in a CMakeLists file using the set command without the CACHE option.
# Command line option: cmake . -DLNET_POLLER=epoll/poll/select
set(LNET_POLLER "" CACHE STRING "Choose polling system, valid values are epoll, poll or select [default=autodetect]")

# Command line option: cmake . -DSTATIC_CARES_LIB=ON
option(STATIC_CARES_LIB "Build c-ares as a static library" ON)
option(LNET_DEBUG "Print debug message to stdout/stderr" OFF)
option(LNET_BUILD_TESTS "Build tests and demos" OFF)

include(FetchContent) # New in version 3.11.

# https://discourse.cmake.org/t/what-is-the-correct-way-to-set-options-of-a-project-before-fetch-content/268
if(STATIC_CARES_LIB)
  message(STATUS "Build c-ares as a static library")
  set(CARES_STATIC ON CACHE BOOL "Build c-ares as a static library")
  set(CARES_SHARED OFF CACHE BOOL "Build c-ares as a shared library")
endif()

FetchContent_Declare(cares
  GIT_REPOSITORY  https://github.com/c-ares/c-ares.git
  GIT_TAG         cares-1_23_0
)
set(THIRD_PARTIES cares)

if(LNET_BUILD_TESTS)
  message(STATUS "Build tests and demos")
  FetchContent_Declare(googletest
    GIT_REPOSITORY    https://github.com/google/googletest
    GIT_TAG           v1.12.0
  )
  FetchContent_Declare(gflags
    GIT_REPOSITORY  https://github.com/gflags/gflags.git
    GIT_TAG         v2.2.2
  )
  list(APPEND THIRD_PARTIES googletest gflags)
endif()

if(LNET_BUILD_TESTS OR LNET_DEBUG)
  FetchContent_Declare(fmt
    GIT_REPOSITORY  https://github.com/fmtlib/fmt.git
    GIT_TAG         10.1.1
  )
  list(APPEND THIRD_PARTIES fmt)
endif()

FetchContent_MakeAvailable(${THIRD_PARTIES})

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
add_subdirectory(dns)
