add_library(address
  "sockaddr.cpp"
)
target_compile_options(address PRIVATE ${MY_CXX_FLAGS})
target_include_directories(address PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(address)

add_executable(sockaddr_test
  "sockaddr_test.cpp"
)
target_compile_options(sockaddr_test PRIVATE ${MY_CXX_FLAGS})
target_link_libraries(sockaddr_test address gtest_main)
