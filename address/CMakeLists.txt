add_library(address
  "sockaddr.cpp"
  "${PROJECT_SOURCE_DIR}/config.h"
)
target_compile_options(address PRIVATE ${MY_CXX_FLAGS})
target_include_directories(address PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(address)

if(LNET_BUILD_TESTS)
  add_executable(sockaddr_test "sockaddr_test.cpp")
  target_compile_options(sockaddr_test PRIVATE ${MY_CXX_FLAGS})
  target_link_libraries(sockaddr_test address gtest_main)
endif()
