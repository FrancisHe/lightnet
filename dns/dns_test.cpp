#include "dns.h"
#include "gtest/gtest.h"
#include "fmt/format.h"
#include <unordered_map>
#include <unordered_set>
#include <memory>

namespace net {
namespace dns {
namespace test {

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

std::string StringifySockAddr(const net::address::SockAddr& addr) {
  auto sa_family = reinterpret_cast<const sockaddr*>(&addr)->sa_family;
  if (sa_family == AF_INET6) {
    const sockaddr_in6* in6 = reinterpret_cast<const sockaddr_in6*>(&addr);
    return fmt::format("family={}, addr={}", Int2NameFamily(sa_family),
                       net::address::ToString(in6->sin6_addr));
  } else {
    const sockaddr_in* in4 = reinterpret_cast<const sockaddr_in*>(&addr);
    return fmt::format("family={}, addr={}", Int2NameFamily(sa_family),
                       net::address::ToString(in4->sin_addr));
  }
}

std::string StringifySockAddrList(const AddrList& addrs) {
  std::string result;
  for (auto& addr : addrs) {
    if (result.empty()) {
      result = StringifySockAddr(addr);
    } else {
      result += "; " + StringifySockAddr(addr);
    }
  }
  return result;
}

struct FakeProxy : public event::EventHandler {
  // Timeout 0 means no timer will be set.
  FakeProxy(event::Poller* poller, AresResolver* resolver,
            std::vector<std::string>&& reqs, uint32_t timeout = 0)
    : poller_(poller), resolver_(resolver), requests_(std::move(reqs)), timeout_(timeout) {
  }
  FakeProxy() = delete;
  ~FakeProxy() = default;

  void StartParallelResolve() {
    for (size_t i = 0; i < requests_.size(); ++i) {
      auto cb = [this, i](ResolveStatus status, AddrList&& addrs) {
        OnResolveComplete(i, status, std::move(addrs));
      };
      auto dns_req = resolver_->Resolve(requests_[i], AddrFamily::kUnSpec, cb);
      if (dns_req) {
        pending_reqs_.emplace(i, dns_req);
      }
    }

    if (timeout_ > 0) {
      poller_->AddTimer(timeout_, this);
    }
  }

  // Simulating request upstream when dns resolution completes.
  void OnResolveComplete(size_t ireq, ResolveStatus status, AddrList&& addrs) {
    pending_reqs_.erase(ireq);
    if (status == kResolveFailure) {
      fail_reqs_.emplace(ireq);
    } else {
      succ_reqs_.emplace(ireq, std::move(addrs));
    }
  }

  void OnTimeout(int id) override {
    for (auto& req : pending_reqs_) {
      req.second->Cancel();
    }
    timedout_ = true;
  }

  void OnReadable(int fd) override {}
  void OnWritable(int fd) override {}

  event::Poller* poller_{nullptr};
  AresResolver* resolver_{nullptr};
  const std::vector<std::string> requests_;
  uint32_t timeout_{0};
  std::unordered_map<size_t, DnsQuery*> pending_reqs_;
  std::unordered_map<size_t, AddrList> succ_reqs_;
  std::unordered_set<size_t> fail_reqs_;
  bool timedout_{false};
};

}  // namespace test
}  // namespace dns
}  // namespace net

#define TEST_NS net::dns::test

GTEST_TEST(DnsTest, CallbackLeakTest) {
  auto poller = std::make_unique<net::event::Poller>();

  net::dns::AresResolver::Options dns_opts;
  dns_opts.use_tcp = true;
  auto resolver = std::make_unique<net::dns::AresResolver>(poller.get(), dns_opts);
  resolver->SetServers("8.8.8.8,8.8.4.4");

  std::vector<std::string> requests = {
    "www.qq.com",
    "localhost",
    "www.sina.com.cn",
    "www.google.com",
    "www.pku.edu.cn",
    "u.ae",
    "www.gov.za",
    "::1"
  };

  // Resolution cannot be done in 50ms.
  auto proxy = std::make_unique<TEST_NS::FakeProxy>(poller.get(), resolver.get(),
                                                    std::move(requests), 50);
  proxy->StartParallelResolve();
  do {
    poller->DoPoll();
  } while (!proxy->timedout_);

  ASSERT_EQ(proxy->succ_reqs_.size(), 2);
  ASSERT_TRUE(proxy->succ_reqs_.count(1));  // localhost
  ASSERT_TRUE(proxy->succ_reqs_.count(7));  // ::1
  ASSERT_EQ(proxy->fail_reqs_.size(), 0);  // 6 requests will be cancelled

  // Test releasing ResolveCb before AresResolver (if AresResolver calls
  // a dangling ResolveCb in ares_destroy(), there will be a segfault).
  proxy.reset();  // release
  resolver.reset(); // release
  poller.reset();  // release
}

GTEST_TEST(DnsTest, ParallelResolveTest) {
  auto poller = std::make_unique<net::event::Poller>();

  net::dns::AresResolver::Options dns_opts;
  dns_opts.use_tcp = true;
  auto resolver = std::make_unique<net::dns::AresResolver>(poller.get(), dns_opts);

  std::vector<std::string> requests = {
    "www.qq.com",
    "localhost",
    "www.sina.com.cn",
    "www.google.com",
    "www.pku.edu.cn",
    "u.ae",
    "www.gov.za",
    "::1"
  };
  auto requests_num = requests.size();

  auto proxy = std::make_unique<TEST_NS::FakeProxy>(poller.get(), resolver.get(), std::move(requests));
  proxy->StartParallelResolve();
  do {
    poller->DoPoll();
  } while (!proxy->pending_reqs_.empty());

  ASSERT_EQ(proxy->succ_reqs_.size(), requests_num);
  ASSERT_EQ(proxy->fail_reqs_.size(), 0);
  ASSERT_EQ(poller->FdCount(), 0);
  ASSERT_EQ(poller->TimerCount(), 0);

  proxy.reset();  // release
  resolver.reset(); // release
  poller.reset();  // release
}

#undef TEST_NS
