set(EVENT_SRCS
  "base_poller.cpp"
  "epoll.cpp"
  "poll.cpp"
  "select.cpp"
  "ticker.cpp"
  "${PROJECT_SOURCE_DIR}/config.h"
)
add_library(event ${EVENT_SRCS})
target_compile_options(event PRIVATE ${MY_CXX_FLAGS})
target_include_directories(event PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(event)

if(LNET_BUILD_TESTS)
  add_executable(ticker_test "ticker_test.cpp")
  target_compile_options(ticker_test PRIVATE ${MY_CXX_FLAGS})
  target_link_libraries(ticker_test event gtest_main)
endif()
