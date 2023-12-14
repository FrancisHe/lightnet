#pragma once
#include <string>
#include <memory>
#include <functional>
#include <list>

#include "event/poller.h"
#include "address/sockaddr.h"
#include "ares.h"

namespace LNETNS {
namespace dns {

enum class AddrFamily {
  kUnSpec,  // AF_UNSPEC
  kInet4,   // AF_INET
  kInet6,   // AF_INET6
};

enum ResolveStatus {
  kResolveSuccess = 0,
  kResolveFailure = -1,
};

// status can be kResolveSuccess or kResolveFailure (kResolvePending is for AresResolver::Resolve)
using AddrList = std::list<address::SockAddr>;
using ResolveCb = std::function<void(ResolveStatus status, AddrList&& addrs)>;

class DnsQuery {
public:
  virtual ~DnsQuery() = default;

  // Make sure Cancel() is being called if query hasn't completed and memory release of
  // ResolveCb's holder prior to AresResolver. Otherwise, AresResolver will call a dangling
  // callback when destroy c-ares channel. 
  virtual void Cancel() = 0;
};

class AresResolver : event::EventHandler {
public:
  struct Options {
    bool use_tcp{false};  // ARES_FLAG_USEVC, always use TCP queries instead of UDP queries
    bool no_search{false};  // ARES_FLAG_NOSEARCH, do not use the default search domains
    int timeout{-1};  // milliseconds, negative means use the c-ares default value
  };

public:
  AresResolver(event::Poller* poller);
  AresResolver(event::Poller* poller, const Options& opt);
  ~AresResolver() override;

  // Use custom servers to replace servers from system configuration or previously
  // set ones. Incomming string format: "host[:port][,host[:port]]...".
  // Note: empty string will clear all configured servers and fallback to use
  // system configured ones.
  bool SetServers(std::string servers);

  // Returns the pending query object if this is a asynchronous resolution, or null
  // if it's a synchronous resolution (e.g. localhost) or failure (callback won't
  // be invoked in this circumstance).
  //
  // Beware of set "af" to AddrFamily::kUnSpec (AF_UNSPEC). If part of query completes
  // (i.e. either record A or record AAAA received), ares_addrinfo_callback won't be
  // invoked when you call ares_cancel/ares_destroy, which will cause memory leak.
  DnsQuery* Resolve(const std::string& name, AddrFamily af, ResolveCb callback);
  const std::string& GetLastError() const;

private:
  struct Query : public DnsQuery {
    Query(AresResolver* rsv, ResolveCb cb) : resolver(rsv), callback(cb) {}
    ~Query() override = default;

    // c-ares only supports channel-wide cancellation, so we just allow the
    // network events to continue but don't invoke the callback on completion.
    void Cancel() override { cancelled_ = true; }

    // ares_getaddrinfo callback.
    void OnGetAddrInfoCallback(int status, int timeouts, ares_addrinfo* addrinfo);

    AresResolver* resolver{nullptr};
    ResolveCb callback;
    bool completed_{false};
    bool cancelled_{false};
    bool owned_{false};  // self-delete when the request completes if owned by itself
  };

private:
  bool InitAresChannel();
  bool ReinitAresChannel();
  void UpdateTimer();

  void OnAresSocketStateChange(int fd, int read, int write);

  void OnReadable(int fd) override;
  void OnWritable(int fd) override;
  void OnTimeout(int id) override;

private:
  event::Poller* poller_{nullptr};
  const Options* options_{nullptr};
  bool use_specified_svrs_{false};

  // It is recommended for an application to have at most one ares channel and
  // use this for all DNS queries for the life of the application.
  ares_channel_t* channel_{nullptr};
  bool dirty_channel_{false};  // reinit the channel if error happens

  event::TimerKey timer_key_{event::kBadTimerKey};
  std::string err_;

  static const Options kDefaultOptions;
};

inline const std::string& AresResolver::GetLastError() const {
  return err_;
}

}  // namespace dns
}  // namespace LNETNS
