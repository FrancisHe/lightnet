#include "dns.h"
#include <iostream>
#include <string>
#include "debug.h"

namespace LNETNS {
namespace dns {

const AresResolver::Options AresResolver::kDefaultOptions;

AresResolver::AresResolver(event::Poller* poller)
  : AresResolver(poller, kDefaultOptions) {
}

AresResolver::AresResolver(event::Poller* poller, const Options& opt)
  : poller_(poller) {
  if (&opt != &kDefaultOptions) {
    options_ = new Options(opt);
  } else {
    options_ = &opt;
  }
}

AresResolver::~AresResolver() {
  // All fds and timers will be removed in OnAresSocketStateChange when calling ares_destroy.
  if (channel_) {
    ares_destroy(channel_);
    channel_ = nullptr;
  }

  if (options_ != &kDefaultOptions) {
    delete options_;
    options_ = nullptr;
  }
}

bool AresResolver::SetServers(std::string servers) {
  if (servers.empty()) {
    if (!use_specified_svrs_) {
      // Previously configured servers are from system configuration.
      return true;
    }

    // c-ares doesn't support setting back to system configured servers.
    if (channel_) {
      ares_destroy(channel_);
      channel_ = nullptr;
    }
    return InitAresChannel();
  }

  if (!channel_) {
    if (!InitAresChannel()) {
      return false;
    }
  }

  // Note: ares_set_servers* will replaces any previously configured name servers
  // with the given ones and set optmask to ARES_OPT_SERVERS (means user specified).
  int rc = ares_set_servers_ports_csv(channel_, servers.c_str());
  if (rc != ARES_SUCCESS) {
    err_ = std::string("ares_set_servers_ports_csv failed: ") + ares_strerror(rc);
    LOG_ERROR("{}", err_);
    return false;
  }

  return true;
}

DnsQuery* AresResolver::Resolve(const std::string& name, AddrFamily af, ResolveCb callback) {
  // TODO: reinit channel periodically.
  if (!channel_) {
    if (!InitAresChannel()) {
      callback(kResolveFailure, {});
      return nullptr;
    }
  } else if (dirty_channel_) {
    LOG_INFO("channel is dirty.");
    if (!ReinitAresChannel()) {
      callback(kResolveFailure, {});
      return nullptr;
    }
  }

  ares_addrinfo_hints hints = {};
  switch (af) {
  case LNETNS::dns::AddrFamily::kInet4:
    hints.ai_family = AF_INET;
    break;
  case LNETNS::dns::AddrFamily::kInet6:
    hints.ai_family = AF_INET6;
    break;
  default:
    hints.ai_family = AF_UNSPEC;
    break;
  }
  hints.ai_flags = ARES_AI_NOSORT;
  auto query = std::make_unique<Query>(this, callback);
  auto cb = [](void* arg, int status, int timeouts, ares_addrinfo* addrinfo) {
    auto dnsq = static_cast<Query*>(arg);
    dnsq->OnGetAddrInfoCallback(status, timeouts, addrinfo);
  };
  ares_getaddrinfo(channel_, name.c_str(), nullptr, &hints, cb, query.get());
  if (query->completed_) {
    // Resolution does not need asynchronous behavior. For example, localhost lookup.
    LOG_DEBUG("ares_getaddrinfo() completes immediately.");
    return nullptr;
  }

  // Asynchronous pending request.
  UpdateTimer();
  query->owned_ = true;  // Make AresDnsQuery being released in OnGetAddrInfoCallback.
  return query.release();
}

bool AresResolver::InitAresChannel() {
  ares_options options;
  int optmask = ARES_OPT_SOCK_STATE_CB;
  options.sock_state_cb = [](void* data, int fd, int read, int write) {
    static_cast<AresResolver*>(data)->OnAresSocketStateChange(fd, read, write);
  };
  options.sock_state_cb_data = this;

  options.flags = 0;
  if (options_->use_tcp) {
    optmask |= ARES_OPT_FLAGS;
    options.flags |= ARES_FLAG_USEVC;
  }
  if (options_->no_search) {
    optmask |= ARES_OPT_FLAGS;
    options.flags |= ARES_FLAG_NOSEARCH;
  }
  if (options_->timeout > 0) {
    optmask |= ARES_OPT_TIMEOUTMS;
    options.timeout = options_->timeout;
  }

  int rc = ares_init_options(&channel_, &options, optmask);
  if (rc != ARES_SUCCESS) {
    if (channel_) {
      ares_destroy(channel_);
      channel_ = nullptr;
    }

    err_ = std::string("ares_init_options failed: ") + ares_strerror(rc);
    LOG_ERROR("{}", err_);
    return false;
  }

  use_specified_svrs_ = false;
  return true;
}

bool AresResolver::ReinitAresChannel() {
  // Maybe the system configuration has been changed. The ares_reinit function re-reads
  // the system configuration and safely applies the configuration to the existing channel.
  // Note: ares_reinit won't update the server list if user specified server has been set
  // (ARES_OPT_SERVERS optmask is set).
  int rc = ares_reinit(channel_);
  if (rc != ARES_SUCCESS) {
    err_ = std::string("ares_reinit failed: ") + ares_strerror(rc);
    LOG_ERROR("{}", err_);
    return false;
  }

  dirty_channel_ = false;
  return true;
}

void AresResolver::UpdateTimer() {
  if (timer_key_ != event::kBadTimerKey) {
    poller_->CancelTimer(timer_key_, this);
  }

  // Get the minimum timeout of pending queries.
  timeval timeout;
  timeval* timeout_ptr = ares_timeout(channel_, nullptr, &timeout);
  if (timeout_ptr) {
    // Note: round up to 1ms to avoid wasting CPU if tv_usec is less than 1000.
    int ms = timeout_ptr->tv_sec * 1000 + (timeout_ptr->tv_usec + 999) / 1000;
    LOG_DEBUG("Set timeout value to {}", ms);
    timer_key_ = poller_->AddTimer(ms, this);
  }
#ifdef DEBUG_BUILD
  else {
    LOG_DEBUG("No pending query.");
    // Cancel timer (already done above).
  }
#endif  // DEBUG_BUILD
}

void AresResolver::OnAresSocketStateChange(int fd, int read, int write) {
  UpdateTimer();

  // Stop tracking events for fd if no more state change events.
  if (read == 0 && write == 0) {
    LOG_DEBUG("Remove fd {}", fd);
    poller_->RemoveFd(fd);
    return;
  }

  int events = 0;
  if (read) {
    events |= event::kEventIn;
  }
  if (write) {
    events |= event::kEventOut;
  }
  LOG_DEBUG("Upsert fd {} with events {}", fd, events);
  poller_->UpsertFd(fd, this, events);
}

void AresResolver::OnReadable(int fd) {
  LOG_DEBUG("Channel is ready for reading: fd={}", fd);
  ares_process_fd(channel_, fd, ARES_SOCKET_BAD);
  UpdateTimer();
}

void AresResolver::OnWritable(int fd) {
  LOG_DEBUG("Channel is ready for writing: fd={}", fd);
  ares_process_fd(channel_, ARES_SOCKET_BAD, fd);
  UpdateTimer();
}

void AresResolver::OnTimeout(int id) {
  LOG_WARN("Query timed out.");
  ares_process_fd(channel_, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
  UpdateTimer();
}

void AresResolver::Query::OnGetAddrInfoCallback(int status, int timeouts, ares_addrinfo* addrinfo) {
  completed_ = true;
#ifdef DEBUG_BUILD
  // host_query.timeouts: number of timeouts we saw for this request
  if (timeouts > 0) {
    LOG_DEBUG("Query timed out {} times.", timeouts);
  }
#endif  // DEBUG_BUILD

  std::list<address::SockAddr> addrs;
  ResolveStatus resolve_status;
  if (status == ARES_SUCCESS) {
    if (addrinfo != nullptr && addrinfo->nodes != nullptr) {
      for (const ares_addrinfo_node* ai = addrinfo->nodes; ai != nullptr; ai = ai->ai_next) {
        auto ai_family = ai->ai_family;
        address::SockAddr addr;
        if (ai_family == AF_INET) {
          // memset(&addr.sockaddr_in, 0, sizeof(addr.sockaddr_in));
          addr.sockaddr_in.sin_family = ai_family;
          addr.sockaddr_in.sin_port = 0;
          addr.sockaddr_in.sin_addr = reinterpret_cast<sockaddr_in*>(ai->ai_addr)->sin_addr;
        } else if (ai_family == AF_INET6) {
          // memset(&addr.sockaddr_in6, 0, sizeof(addr.sockaddr_in6));
          addr.sockaddr_in6.sin6_family = ai_family;
          addr.sockaddr_in6.sin6_port = 0;
          addr.sockaddr_in6.sin6_addr = reinterpret_cast<sockaddr_in6*>(ai->ai_addr)->sin6_addr;
        }
        addrs.emplace_back(addr);
      }
    }
    if (!addrs.empty()) {
      resolve_status = kResolveSuccess;
    } else {
      resolve_status = kResolveFailure;
    }

    // c-ares will internally call ares_freeaddrinfo() if status is not ARES_SUCCESS.
    ares_freeaddrinfo(addrinfo);
  } else {
    LOG_ERROR("ares_addrinfo_callback notified error: {}", ares_strerror(status));
    resolve_status = kResolveFailure;

    // c-ares returns ARES_ECONNREFUSED for all error about network operations.
    if (status == ARES_ECONNREFUSED) {
      // If c-ares returns ARES_ECONNREFUSED (all servers have been retried and still failed)
      // we assume that the channel is broken. Mark the channel dirty so that it can be
      // reinitialized on a subsequent call to AresResolver::Resolve().
      //
      // Since this is a callback, the channel cannot be destroyed or reinitialized here.
      if (!resolver->use_specified_svrs_) {
        resolver->dirty_channel_ = true;  // need reinit
      }
    }
  }

  if (!cancelled_) {
    callback(resolve_status, std::move(addrs));
  }
  if (owned_) {
    delete this;
    LOG_DEBUG("Self-deleted AresResolver::DnsQuery.");
  }
}

}  // namespace dns
}  // namespace LNETNS

