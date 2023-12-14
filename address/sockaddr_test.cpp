#include "sockaddr.h"
#include "gtest/gtest.h"

#include <cstring>

GTEST_TEST(AddrTest, JoinHostPortTest) {
  auto addr = LNETNS::address::JoinHostPort("127.0.0.1", 80);
  EXPECT_EQ(addr, "127.0.0.1:80");

  addr = LNETNS::address::JoinHostPort("2001:4860:4860::8888", 53);  // Google Public DNS
  EXPECT_EQ(addr, "[2001:4860:4860::8888]:53");

  addr = LNETNS::address::JoinHostPort("[2001:4860:4860::8888]", 53);
  EXPECT_EQ(addr, "[2001:4860:4860::8888]:53");

  addr = LNETNS::address::JoinHostPort("www.wikipedia.org", 443);
  EXPECT_EQ(addr, "www.wikipedia.org:443");
}

GTEST_TEST(AddrTest, SplitHostPortTest) {
  std::string host;
  std::string port_str;
  bool has_port;
  ASSERT_FALSE(LNETNS::address::SplitHostPort("[2001:4860:4860::8888", &host, &port_str, &has_port));

  // invalid
  ASSERT_TRUE(LNETNS::address::SplitHostPort("2001:4860:4860::8888]", &host, &port_str, &has_port));
  EXPECT_EQ(host, "2001:4860:4860::8888]");
  EXPECT_FALSE(has_port);

  // well-formed
  ASSERT_TRUE(LNETNS::address::SplitHostPort("[2001:4860:4860::8888]", &host, &port_str, &has_port));
  EXPECT_EQ(host, "2001:4860:4860::8888");
  EXPECT_FALSE(has_port);

  // well-formed
  // https://en.wikipedia.org/wiki/IPv6_address#Literal_IPv6_addresses_in_network_resource_identifiers
  ASSERT_TRUE(LNETNS::address::SplitHostPort("[2001:4860:4860::8888]:53", &host, &port_str, &has_port));
  EXPECT_EQ(host, "2001:4860:4860::8888");
  EXPECT_TRUE(has_port);
  EXPECT_EQ(port_str, "53");

  // well-formed
  ASSERT_TRUE(LNETNS::address::SplitHostPort("2001:4860:4860::8888", &host, &port_str, &has_port));
  EXPECT_EQ(host, "2001:4860:4860::8888");
  EXPECT_FALSE(has_port);

  // Note: ill-formed
  ASSERT_TRUE(LNETNS::address::SplitHostPort("2001:4860:4860::8888:53", &host, &port_str, &has_port));
  EXPECT_EQ(host, "2001:4860:4860::8888:53");
  EXPECT_FALSE(has_port);

  // well-formed
  ASSERT_TRUE(LNETNS::address::SplitHostPort("127.0.0.1", &host, &port_str, &has_port));
  EXPECT_EQ(host, "127.0.0.1");
  EXPECT_FALSE(has_port);

  // well-formed
  ASSERT_TRUE(LNETNS::address::SplitHostPort("www.wikipedia.org:443", &host, &port_str, &has_port));
  EXPECT_EQ(host, "www.wikipedia.org");
  EXPECT_TRUE(has_port);
  EXPECT_EQ(port_str, "443");

  // ill-formed
  ASSERT_TRUE(LNETNS::address::SplitHostPort("127.0.0.1:80a", &host, &port_str, &has_port));
  EXPECT_EQ(host, "127.0.0.1");
  EXPECT_TRUE(has_port);
  EXPECT_EQ(port_str, "80a");
}

GTEST_TEST(AddrTest, SplitHostValidatePortTest) {
  std::string host;
  int port;
  ASSERT_FALSE(LNETNS::address::SplitHostPort("[2001:4860:4860::8888]", &host, &port));

  ASSERT_TRUE(LNETNS::address::SplitHostPort("[2001:4860:4860::8888]:53", &host, &port));
  EXPECT_EQ(host, "2001:4860:4860::8888");
  EXPECT_EQ(port, 53);

  ASSERT_TRUE(LNETNS::address::SplitHostPort("www.wikipedia.org:443", &host, &port));
  EXPECT_EQ(host, "www.wikipedia.org");
  EXPECT_EQ(port, 443);

  ASSERT_FALSE(LNETNS::address::SplitHostPort("127.0.0.1:80a", &host, &port));
  ASSERT_FALSE(LNETNS::address::SplitHostPort("127.0.0.1:-1", &host, &port));
  ASSERT_FALSE(LNETNS::address::SplitHostPort("127.0.0.1:65536", &host, &port));

  // think 0 is a valid port
  ASSERT_TRUE(LNETNS::address::SplitHostPort("127.0.0.1:0", &host, &port));
  EXPECT_EQ(host, "127.0.0.1");
  EXPECT_EQ(port, 0);
}

GTEST_TEST(AddrTest, ValidateIPTest) {
  ASSERT_TRUE(LNETNS::address::IsValidIP("2001:4860:4860::8888"));
  ASSERT_TRUE(LNETNS::address::IsValidIP("::"));  // IN6ADDR_ANY_INIT (in6addr_any)
  ASSERT_TRUE(LNETNS::address::IsValidIP("::1"));  // IN6ADDR_LOOPBACK_INIT (in6addr_loopback)
  ASSERT_TRUE(LNETNS::address::IsValidIP("127.0.0.1"));
  ASSERT_TRUE(LNETNS::address::IsValidIP("0.0.0.0"));

  // Note: cannot include '[]'
  ASSERT_FALSE(LNETNS::address::IsValidIP("[2001:4860:4860::8888]"));

  // cannot include port
  ASSERT_FALSE(LNETNS::address::IsValidIP("[2001:4860:4860::8888]:53"));
  ASSERT_FALSE(LNETNS::address::IsValidIP("127.0.0.1:80"));

  ASSERT_FALSE(LNETNS::address::IsValidIP("2001:4860:4860"));
  ASSERT_FALSE(LNETNS::address::IsValidIP("127.0.0"));
}

GTEST_TEST(AddrTest, ParseIPTest) {
  LNETNS::address::SockAddr in4;
  reinterpret_cast<sockaddr_in*>(&in4)->sin_family = AF_INET;
  EXPECT_EQ(reinterpret_cast<sockaddr*>(&in4)->sa_family, AF_INET);

  LNETNS::address::SockAddr in6;
  reinterpret_cast<sockaddr_in6*>(&in6)->sin6_family = AF_INET6;
  EXPECT_EQ(reinterpret_cast<sockaddr*>(&in6)->sa_family, AF_INET6);

  auto sa = LNETNS::address::ParseIP("2001:4860:4860::8888");
  ASSERT_TRUE(sa);
  EXPECT_EQ(reinterpret_cast<sockaddr*>(sa.get())->sa_family, AF_INET6);

  sa = LNETNS::address::ParseIP("::");
  ASSERT_TRUE(sa);
  EXPECT_EQ(reinterpret_cast<sockaddr*>(sa.get())->sa_family, AF_INET6);

  sa = LNETNS::address::ParseIP("::1");
  ASSERT_TRUE(sa);
  EXPECT_EQ(reinterpret_cast<sockaddr*>(sa.get())->sa_family, AF_INET6);

  sa = LNETNS::address::ParseIP("127.0.0.1");
  ASSERT_TRUE(sa);
  EXPECT_EQ(reinterpret_cast<sockaddr*>(sa.get())->sa_family, AF_INET);

  sa = LNETNS::address::ParseIP("0.0.0.0");
  ASSERT_TRUE(sa);
  EXPECT_EQ(reinterpret_cast<sockaddr*>(sa.get())->sa_family, AF_INET);
}

GTEST_TEST(AddrTest, ParseIPPortTest) {
  EXPECT_FALSE(LNETNS::address::ParseIPPort("[2001:4860:4860::8888"));
  EXPECT_FALSE(LNETNS::address::ParseIPPort("2001:4860:4860::8888]"));

  // think as "ipv4:port"
  EXPECT_FALSE(LNETNS::address::ParseIPPort("2001:4860:4860::8888"));
  EXPECT_FALSE(LNETNS::address::ParseIPPort("2001:4860:4860::8888:53"));

  // no port supplied
  EXPECT_FALSE(LNETNS::address::ParseIPPort("[2001:4860:4860::8888]"));
  EXPECT_FALSE(LNETNS::address::ParseIPPort("127.0.0.1"));

  // invalid port
  EXPECT_FALSE(LNETNS::address::ParseIPPort("127.0.0.1:80a"));
  EXPECT_FALSE(LNETNS::address::ParseIPPort("127.0.0.1:65536"));
  EXPECT_FALSE(LNETNS::address::ParseIPPort("[2001:4860:4860::8888]:53a"));
  EXPECT_FALSE(LNETNS::address::ParseIPPort("[2001:4860:4860::8888]:-1"));

  // domain is invalid
  EXPECT_FALSE(LNETNS::address::ParseIPPort("www.wikipedia.org:443"));

  auto sa = LNETNS::address::ParseIPPort("[2001:4860:4860::8888]:53");
  ASSERT_TRUE(sa);
  EXPECT_EQ(reinterpret_cast<sockaddr_in6*>(sa.get())->sin6_family, AF_INET6);
  EXPECT_EQ(reinterpret_cast<sockaddr_in6*>(sa.get())->sin6_port, htons(53));
  EXPECT_EQ(LNETNS::address::ToString(*sa), "[2001:4860:4860::8888]:53");

  sa = LNETNS::address::ParseIPPort("[::1]:53");
  ASSERT_TRUE(sa);
  EXPECT_EQ(reinterpret_cast<sockaddr_in6*>(sa.get())->sin6_family, AF_INET6);
  EXPECT_EQ(reinterpret_cast<sockaddr_in6*>(sa.get())->sin6_port, htons(53));
  auto sin6_addr = &reinterpret_cast<sockaddr_in6*>(sa.get())->sin6_addr;
  EXPECT_EQ(std::memcmp(sin6_addr, &in6addr_loopback, sizeof(in6_addr)), 0);
  EXPECT_EQ(LNETNS::address::ToString(*sa), "[::1]:53");

  sa = LNETNS::address::ParseIPPort("[::]:53");
  ASSERT_TRUE(sa);
  EXPECT_EQ(reinterpret_cast<sockaddr_in6*>(sa.get())->sin6_family, AF_INET6);
  EXPECT_EQ(reinterpret_cast<sockaddr_in6*>(sa.get())->sin6_port, htons(53));
  sin6_addr = &reinterpret_cast<sockaddr_in6*>(sa.get())->sin6_addr;
  EXPECT_EQ(std::memcmp(sin6_addr, &in6addr_any, sizeof(in6_addr)), 0);
  EXPECT_EQ(LNETNS::address::ToString(*sa), "[::]:53");

  sa = LNETNS::address::ParseIPPort("127.0.0.1:80");
  ASSERT_TRUE(sa);
  EXPECT_EQ(reinterpret_cast<sockaddr_in*>(sa.get())->sin_family, AF_INET);
  EXPECT_EQ(reinterpret_cast<sockaddr_in*>(sa.get())->sin_port, htons(80));
  EXPECT_EQ(reinterpret_cast<sockaddr_in*>(sa.get())->sin_addr.s_addr, htonl(INADDR_LOOPBACK));
  EXPECT_EQ(LNETNS::address::ToString(*sa), "127.0.0.1:80");

  sa = LNETNS::address::ParseIPPort("0.0.0.0:80");
  ASSERT_TRUE(sa);
  EXPECT_EQ(reinterpret_cast<sockaddr_in*>(sa.get())->sin_family, AF_INET);
  EXPECT_EQ(reinterpret_cast<sockaddr_in*>(sa.get())->sin_port, htons(80));
  EXPECT_EQ(reinterpret_cast<sockaddr_in*>(sa.get())->sin_addr.s_addr, htonl(INADDR_ANY));
  EXPECT_EQ(LNETNS::address::ToString(*sa), "0.0.0.0:80");
}
