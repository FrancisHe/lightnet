set(LIB_DNS dns)
set(LIB_DNS_OUTPUT_NAME lightnet-dns)

# Library type (SHARED or STATIC) is determined internally by BUILD_SHARED_LIBS.
add_library(${LIB_DNS} "dns.cpp")
target_compile_options(${LIB_DNS} PRIVATE ${MY_CXX_FLAGS})
target_include_directories(${LIB_DNS} PUBLIC ${PROJECT_SOURCE_DIR})
set(DNS_LINK_LIBS lightnet::event lightnet::address c-ares::cares)
if(LNET_DEBUG)
  list(APPEND DNS_LINK_LIBS fmt::fmt)
endif()
target_link_libraries(${LIB_DNS} ${DNS_LINK_LIBS})

set_target_properties(${LIB_DNS} PROPERTIES
  OUTPUT_NAME ${LIB_DNS_OUTPUT_NAME}
)
add_library(lightnet::${LIB_DNS} ALIAS ${LIB_DNS})

if(LNET_BUILD_TESTS)
  add_executable(resolver-demo "resolver-demo.cpp")
  target_compile_options(resolver-demo PRIVATE ${MY_CXX_FLAGS})
  target_link_libraries(resolver-demo lightnet::dns gflags::gflags fmt::fmt)

  add_executable(dns_test "dns_test.cpp")
  target_compile_options(dns_test PRIVATE ${MY_CXX_FLAGS})
  target_link_libraries(dns_test lightnet::dns gtest_main fmt::fmt)
endif()
