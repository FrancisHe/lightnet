set(LIB_ADDR address)
set(LIB_ADDR_OUTPUT_NAME lightnet-address)

# Library type (SHARED or STATIC) is determined internally by BUILD_SHARED_LIBS.
add_library(${LIB_ADDR}
  "sockaddr.cpp"
  "${PROJECT_SOURCE_DIR}/config.h"
)
target_compile_options(${LIB_ADDR} PRIVATE ${MY_CXX_FLAGS})
target_include_directories(${LIB_ADDR} PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(${LIB_ADDR})

set_target_properties(${LIB_ADDR} PROPERTIES
  OUTPUT_NAME ${LIB_ADDR_OUTPUT_NAME}
)
add_library(lightnet::${LIB_ADDR} ALIAS ${LIB_ADDR})

if(LNET_BUILD_TESTS)
  add_executable(sockaddr_test "sockaddr_test.cpp")
  target_compile_options(sockaddr_test PRIVATE ${MY_CXX_FLAGS})
  target_link_libraries(sockaddr_test lightnet::address gtest_main)
endif()
