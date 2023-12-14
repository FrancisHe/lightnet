#include <iostream>
#include <string>

#include "gflags/gflags.h"
#include "fmt/format.h"
#include "dns.h"

DEFINE_string(host, "", "Hostname or domain.");
DEFINE_bool(use_tcp, false, "Always use TCP queries.");
DEFINE_int32(timeout, -1, "Query timeout in milliseconds, <= 0 means use c-ares default.");
DEFINE_string(servers, "", "Custom name servers, use cvs format.");
DEFINE_bool(debug, false, "Debug mode for print details.");

static const char* kModeSync = "SYNC";
static const char* kModeAsync = "ASYNC";

struct ResolverDemo {
  void OnComplete(LNETNS::dns::ResolveStatus status, LNETNS::dns::AddrList&& addrs);

  bool completed{false};
  LNETNS::dns::ResolveStatus status{LNETNS::dns::kResolveFailure};
  LNETNS::dns::AddrList addrs;
};

void ResolverDemo::OnComplete(LNETNS::dns::ResolveStatus status, LNETNS::dns::AddrList&& addrs) {
  this->status = status;
  this->addrs = std::move(addrs);
  completed = true;
}

LNETNS::dns::ResolveCb MakeResolverDemoCallback(ResolverDemo& demo) {
  using std::placeholders::_1;
  using std::placeholders::_2;
  return std::bind(&ResolverDemo::OnComplete, demo, _1, _2);
}

LNETNS::dns::ResolveCb MakeResolverDemoCallback(ResolverDemo* demo) {
  using std::placeholders::_1;
  using std::placeholders::_2;
  // std::bind accept both a reference and a pointer.
  return std::bind(&ResolverDemo::OnComplete, demo, _1, _2);
}

LNETNS::dns::ResolveCb MakeResolverDemoCallbackLambda(ResolverDemo& demo) {
  return [&demo](LNETNS::dns::ResolveStatus status, LNETNS::dns::AddrList&& addrs) {
    demo.OnComplete(status, std::move(addrs));
  };
}

LNETNS::dns::ResolveCb MakeResolverDemoCallbackLambda(ResolverDemo* demo) {
  return [&demo](LNETNS::dns::ResolveStatus status, LNETNS::dns::AddrList&& addrs) {
    demo->OnComplete(status, std::move(addrs));
  };
}

std::string Int2NameFamily(int family) {
  switch (family) {
  case AF_INET:
    return "AF_INET";
  case AF_INET6:
    return "AF_INET6";
  default:
    return std::to_string(family);
  }
}

std::string StringifySockAddr(const LNETNS::address::SockAddr& addr) {
  auto sa_family = reinterpret_cast<const sockaddr*>(&addr)->sa_family;
  if (sa_family == AF_INET6) {
    const sockaddr_in6* in6 = reinterpret_cast<const sockaddr_in6*>(&addr);
    return fmt::format("family={}, addr={}", Int2NameFamily(sa_family),
                       LNETNS::address::ToString(in6->sin6_addr));
  } else {
    const sockaddr_in* in4 = reinterpret_cast<const sockaddr_in*>(&addr);
    return fmt::format("family={}, addr={}", Int2NameFamily(sa_family),
                       LNETNS::address::ToString(in4->sin_addr));
  }
}

int main(int argc, char* argv[]) {
  std::string usage = "Usage: " + std::string(argv[0]) + " [options]";
  gflags::SetUsageMessage(usage);
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  LNETNS::event::Poller dispatcher;
  if (!dispatcher) {
    fmt::println(stderr, "Create dispatcher failed: {}",
                 strerror(dispatcher.GetLastErrno()));
    return 1;
  }

  LNETNS::dns::AresResolver::Options dns_opts;
  dns_opts.use_tcp = FLAGS_use_tcp;
  dns_opts.timeout = FLAGS_timeout;
  LNETNS::dns::AresResolver resolver(&dispatcher, dns_opts);
  resolver.SetServers(FLAGS_servers);

  ResolverDemo demo;
  const char* mode = kModeAsync;
  auto dns_query = resolver.Resolve(FLAGS_host, LNETNS::dns::AddrFamily::kUnSpec,
                                    MakeResolverDemoCallback(&demo));
  if (!dns_query) {
    if (!demo.completed) {
      fmt::println(stderr, "Resolve failed: {}", resolver.GetLastError());
      return 2;
    }
    mode = kModeSync;
  }

  while (!demo.completed) {
    auto nevents = dispatcher.DoPoll();
    if (FLAGS_debug) {
      fmt::println("Processed {} events.", nevents);
    }
  }
  if (FLAGS_debug && (dispatcher.FdCount() || dispatcher.TimerCount())) {
    fmt::println("The dispatcher still has {} fds and {} timers",
                 dispatcher.FdCount(), dispatcher.TimerCount());
  }

  if (demo.status == LNETNS::dns::kResolveFailure) {
    fmt::println(stderr, "Resolve failed");
    return 3;
  }

  for (auto& addr : demo.addrs) {
    fmt::println("{}: {}", mode, StringifySockAddr(addr));
  }

  return 0;
}
