set(DNS_SRCS
  "dns.cpp"
)
add_library(dns ${DNS_SRCS})
target_compile_options(dns PRIVATE ${MY_CXX_FLAGS})
target_include_directories(dns PUBLIC ${PROJECT_SOURCE_DIR})
set(DNS_LINK_LIBS event address c-ares::cares)
if(ENABLE_DEBUG)
  list(APPEND DNS_LINK_LIBS fmt::fmt)
endif()
target_link_libraries(dns ${DNS_LINK_LIBS})

set(CARES_GETADDRINFO_SRCS
  "resolver-demo.cpp"
)
add_executable(resolver-demo ${CARES_GETADDRINFO_SRCS})
target_compile_options(resolver-demo PRIVATE ${MY_CXX_FLAGS})
target_link_libraries(resolver-demo dns gflags::gflags fmt::fmt)

add_executable(dns_test
  "dns_test.cpp"
)
target_compile_options(dns_test PRIVATE ${MY_CXX_FLAGS})
target_link_libraries(dns_test dns gtest_main fmt::fmt)