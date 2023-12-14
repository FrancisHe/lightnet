cmake_minimum_required (VERSION 3.14)
project ("net" LANGUAGES C CXX)
set (CMAKE_CXX_STANDARD 17)

set(MY_CXX_FLAGS -Wall -fPIC -g)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# CMake Option:
# https://cmake.org/cmake/help/book/mastering-cmake/chapter/CMake%20Cache.html
# https://cmake.org/cmake/help/latest/command/option.html
# https://stackoverflow.com/questions/8709877/cmake-string-options

# If a variable is in the cache, it can still be overridden in a CMakeLists file using the set command without the CACHE option.
# Command line option: cmake . -DPOLLER=epoll/poll/select
set(POLLER "" CACHE STRING "Choose polling system, valid values are epoll, poll or select [default=autodetect]")

# If <variable> is already set as a normal or cache variable, then the command does nothing.
# Command line option: cmake . -DSTATIC_CARES_LIB=ON
option(STATIC_CARES_LIB "Build c-ares as a static library" ON)
if(STATIC_CARES_LIB)
  message(STATUS "Build c-ares as a static library")
  set(CARES_STATIC ON)
  set(CARES_SHARED OFF)
endif()

option(ENABLE_DEBUG "Print debug message to stdout/stderr" OFF)
if(ENABLE_DEBUG)
  message(STATUS "Build with debug mode")
  set(DEBUG_BUILD 1)
endif()

include(FetchContent) # New in version 3.11.

FetchContent_Declare(googletest
  GIT_REPOSITORY    https://github.com/google/googletest
  GIT_TAG           v1.12.0
)
FetchContent_Declare(gflags
  GIT_REPOSITORY  https://github.com/gflags/gflags.git
  GIT_TAG         v2.2.2
)
FetchContent_Declare(fmt
  GIT_REPOSITORY  https://github.com/fmtlib/fmt.git
  GIT_TAG         10.1.1
)
FetchContent_Declare(cares
  GIT_REPOSITORY  https://github.com/c-ares/c-ares.git
  GIT_TAG         cares-1_23_0
)
FetchContent_MakeAvailable(googletest gflags fmt cares)

# Refer to https://github.com/zeromq/libzmq/blob/master/CMakeLists.txt
include(CheckCSourceRuns) # check_c_source_runs
include(CheckCSourceCompiles) # check_c_source_compiles

list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULES_DIR ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND CMAKE_MODULE_PATH ${CMAKE_MODULES_DIR})
include(SourceRunChecks)

if(POLLER STREQUAL "")
  check_cxx_symbol_exists(epoll_create sys/epoll.h HAVE_EPOLL)
  if(HAVE_EPOLL)
    set(POLLER "epoll")
    check_cxx_symbol_exists(epoll_create1 sys/epoll.h HAVE_EPOLL_CLOEXEC)
    if(HAVE_EPOLL_CLOEXEC)
      set(POLLER_USE_EPOLL_CLOEXEC 1)
    endif()
  endif()
endif()

if(POLLER STREQUAL "")
  check_cxx_symbol_exists(poll poll.h HAVE_POLL)
  if(HAVE_POLL)
    set(POLLER "poll")
  endif()
endif()

if(POLLER STREQUAL "")
  check_cxx_symbol_exists(select sys/select.h HAVE_SELECT)
  if(HAVE_SELECT)
    set(POLLER "select")
  else()
    message(FATAL_ERROR "Could not autodetect polling method")
  endif()
endif()

if(POLLER STREQUAL "epoll" OR POLLER STREQUAL "poll" OR POLLER STREQUAL "select")
  message(STATUS "Using polling method: ${POLLER}")
  string(TOUPPER ${POLLER} UPPER_POLLER)
  set(POLLER_USE_${UPPER_POLLER} 1)
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
configure_file(config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/config.h)

add_subdirectory(event)
add_subdirectory(address)
add_subdirectory(dns)
