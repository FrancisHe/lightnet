#pragma once
#include "config.h"

#include <arpa/inet.h>
#include <sys/un.h>  // sockaddr_un

#include <string>
#include <memory>

namespace LNETNS {
namespace address {

// see "socket/socket-api.md"
// refer to 'ngx_sockaddr_t'
union SockAddr {
  struct sockaddr     sockaddr;      // 16 bytes
  struct sockaddr_in  sockaddr_in;   // 16 bytes
  struct sockaddr_in6 sockaddr_in6;  // 28 bytes
  struct sockaddr_un  sockaddr_un;   // 110 bytes
};

// Given a host and port, creates a newly-allocated string of the form
// "host:port" or "[ho:st]:port", depending on whether the host contains colons
// like an IPv6 literal.  If the host is already bracketed, then additional
// brackets will not be added.
std::string JoinHostPort(const std::string& host, int port);

// Given a name in the form "host:port" or "[ho:st]:port", split into hostname and port number.
bool SplitHostPort(const std::string& addr, std::string* host, std::string* port, bool* has_port);

// Must has valid port.
bool SplitHostPort(const std::string& addr, std::string* host, int* port);

// Works both for IPv4 and IPv6.
std::shared_ptr<SockAddr> ParseIP(const char* ip);
bool IsValidIP(const char* ip);

// "ipv4:port" or "[ipv6]:port"
std::shared_ptr<SockAddr> ParseIPPort(const std::string& addr);

// sockaddr to human-readable ip (and port) string, e.g. "ipv6", "[ipv6]:port"
std::string ToString(const in_addr& sa);
std::string ToString(const in6_addr& sa);
std::string ToString(const SockAddr& sa, bool iponly = false);

}  // namespace address
}  // namespace LNETNS
