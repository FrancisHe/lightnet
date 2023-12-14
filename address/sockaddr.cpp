#include "sockaddr.h"

#include <cstdio>
#include <cstring>
#include <memory>

namespace net {
namespace address {

namespace {

bool ParsePort(const std::string& s, int* port) {
  // https://github.com/envoyproxy/envoy/blob/v1.22.11/source/common/network/utility.cc
  std::size_t pos{};
  try {
    auto a = std::stoi(s, &pos);  // since C++11
    if (pos < s.size()) {  // training non-digit characters
      return false;
    }
    if (a < 0 || a > 65535) {
      return false;
    }
    *port = a;
  } catch (const std::invalid_argument& e) {
    return false;
  } catch (const std::out_of_range& e) {
    return false;
  }

  return true;
}

}  // unnamed namespace

// https://github.com/grpc/grpc/blob/v1.59.1/src/core/lib/gprpp/host_port.cc
std::string JoinHostPort(const std::string& host, int port) {
  const char* fmt = nullptr;
  if (!host.empty() && host[0] != '[' && host.rfind(':') != host.npos) {
    // IPv6 literals must be enclosed in brackets.
    fmt = "[%s]:%d";
  } else {
    // Ordinary non-bracketed host:port.
    fmt = "%s:%d";
  }
  auto sz = std::snprintf(nullptr, 0, fmt, host.c_str(), port) + 1;
  auto buf = std::make_unique<char[]>(sz);  // since C++11
  sz = std::snprintf(buf.get(), sz, fmt, host.c_str(), port);
  return std::string(buf.get(), sz);
}

// https://github.com/grpc/grpc/blob/v1.59.1/src/core/lib/gprpp/host_port.cc
// https://en.wikipedia.org/wiki/IPv6_address#Literal_IPv6_addresses_in_network_resource_identifiers
bool SplitHostPort(const std::string& addr, std::string* host, std::string* port, bool* has_port) {
  *has_port = false;
  if (!addr.empty() && addr[0] == '[') {
    // Parse a bracketed host, typically an IPv6 literal.
    const size_t rbracket = addr.find(']', 1);
    if (rbracket == std::string::npos) {
      // Unmatched [
      return false;
    }
    if (rbracket == addr.size() - 1) {
      // ]<end>
      *port = "";
    } else if (addr[rbracket + 1] == ':') {
      // ]:<port?>
      *port = addr.substr(rbracket + 2, addr.size() - rbracket - 2);
      *has_port = true;
    } else {
      // ]<invalid>
      return false;
    }
    *host = addr.substr(1, rbracket - 1);
    if (host->find(':') == std::string::npos) {
      // Require all bracketed hosts to contain a colon, because a hostname or
      // IPv4 address should never use brackets.
      *host = "";
      return false;
    }
  } else {
    size_t colon = addr.find(':');
    if (colon != std::string::npos && addr.find(':', colon + 1) == std::string::npos) {
      // Exactly 1 colon.  Split into host:port.
      *host = addr.substr(0, colon);
      *port = addr.substr(colon + 1, addr.size() - colon - 1);
      *has_port = true;
    } else {
      // 0 or 2+ colons.  Bare hostname or IPv6 litearal.
      *host = addr;
      *port = "";
    }
  }

  return true;
}

bool SplitHostPort(const std::string& addr, std::string* host, int* port) {
  bool has_port;
  std::string port_str;
  auto valid = SplitHostPort(addr, host, &port_str, &has_port);
  if (!valid || !has_port || port_str.empty()) {
    return false;
  }

  if (!ParsePort(port_str, port)) {
    return false;
  }

  return true;
}

std::shared_ptr<SockAddr> ParseIP(const char* ip) {
  if (!ip) {
    return nullptr;
  }

  // https://github.com/envoyproxy/envoy/blob/v1.22.11/source/common/network/utility.cc
  auto sa = std::make_shared<SockAddr>();
  std::memset(sa.get(), 0, sizeof(SockAddr));
  if (inet_pton(AF_INET, ip, &(reinterpret_cast<sockaddr_in*>(sa.get())->sin_addr)) == 1) {
    reinterpret_cast<sockaddr_in*>(sa.get())->sin_family = AF_INET;
    return sa;
  }
  if (inet_pton(AF_INET6, ip, &(reinterpret_cast<sockaddr_in6*>(sa.get())->sin6_addr)) == 1) {
    reinterpret_cast<sockaddr_in6*>(sa.get())->sin6_family = AF_INET6;
    return sa;
  }

  return nullptr;
}

bool IsValidIP(const char* ip) {
  return ParseIP(ip) != nullptr;
}

// https://github.com/envoyproxy/envoy/blob/v1.22.11/source/common/network/utility.cc
std::shared_ptr<SockAddr> ParseIPPort(const std::string& addr) {
  if (addr.empty()) {
    return nullptr;
  }

  auto sa = std::make_shared<SockAddr>();
  std::memset(sa.get(), 0, sizeof(SockAddr));
  if (addr[0] == '[') {
    // Appears to be an IPv6 address. Find the "]:" that separates the address from the port.
    const auto pos = addr.rfind("]:");
    if (pos == std::string::npos) {
      return nullptr;
    }
    const auto ip_str = addr.substr(1, pos - 1);
    const auto port_str = addr.substr(pos + 2);
    int port = 0;
    if (port_str.empty() || !ParsePort(port_str, &port)) {
      return nullptr;
    }

    sockaddr_in6* sa6 = reinterpret_cast<sockaddr_in6*>(sa.get());
    if (ip_str.empty() || inet_pton(AF_INET6, ip_str.c_str(), &sa6->sin6_addr) != 1) {
      return nullptr;
    }
    sa6->sin6_family = AF_INET6;
    sa6->sin6_port = htons(port);
    return sa;
  }

  // Treat it as an IPv4 address followed by a port.
  const auto pos = addr.rfind(':');
  if (pos == std::string::npos) {
    return nullptr;
  }
  const auto ip_str = addr.substr(0, pos);
  const auto port_str = addr.substr(pos + 1);
  int port = 0;
  if (port_str.empty() || !ParsePort(port_str, &port)) {
    return nullptr;
  }

  sockaddr_in* sa4 = reinterpret_cast<sockaddr_in*>(sa.get());
  if (ip_str.empty() || inet_pton(AF_INET, ip_str.c_str(), &sa4->sin_addr) != 1) {
    return nullptr;
  }
  sa4->sin_family = AF_INET;
  sa4->sin_port = htons(port);
  return sa;
}

std::string ToString(const in_addr& sa) {
  char buf[INET_ADDRSTRLEN];
  return inet_ntop(AF_INET, &sa, buf, INET_ADDRSTRLEN);
}

// https://github.com/envoyproxy/envoy/blob/v1.22.11/source/common/network/address_impl.cc
std::string ToString(const in6_addr& sa) {
  char buf[INET6_ADDRSTRLEN];
  return inet_ntop(AF_INET6, &sa, buf, INET6_ADDRSTRLEN);
}

std::string ToString(const SockAddr& sa, bool iponly) {
  auto af = reinterpret_cast<const sockaddr*>(&sa)->sa_family;
  switch (af) {
  case AF_INET: {
    auto sa4 = reinterpret_cast<const sockaddr_in*>(&sa);
    auto ip = ToString(sa4->sin_addr);
    if (iponly) {
      return std::move(ip);
    }
    return JoinHostPort(ip, ntohs(sa4->sin_port));
  }
  case AF_INET6: {
    auto sa6 = reinterpret_cast<const sockaddr_in6*>(&sa);
    auto ip = ToString(sa6->sin6_addr);
    if (iponly) {
      return std::move(ip);
    }
    return JoinHostPort(ip, ntohs(sa6->sin6_port));
  }
  default: {
    return "";
  }
  }
}

}  // namespace address
}  // namespace net
