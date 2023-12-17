set(LIB_EVENT event)
set(LIB_EVENT_OUTPUT_NAME lightnet-event)
set(EVENT_SRCS
  "base_poller.cpp"
  "epoll.cpp"
  "poll.cpp"
  "select.cpp"
  "ticker.cpp"
  "${PROJECT_SOURCE_DIR}/config.h"
)

# Library type (SHARED or STATIC) is determined internally by BUILD_SHARED_LIBS.
add_library(${LIB_EVENT} ${EVENT_SRCS})
target_compile_options(${LIB_EVENT} PRIVATE ${MY_CXX_FLAGS})
target_include_directories(${LIB_EVENT} PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(${LIB_EVENT})

set_target_properties(${LIB_EVENT} PROPERTIES
  OUTPUT_NAME ${LIB_EVENT_OUTPUT_NAME}
)
add_library(lightnet::${LIB_EVENT} ALIAS ${LIB_EVENT})

if(LNET_BUILD_TESTS)
  add_executable(ticker_test "ticker_test.cpp")
  target_compile_options(ticker_test PRIVATE ${MY_CXX_FLAGS})
  target_link_libraries(ticker_test lightnet::event gtest_main)
endif()
