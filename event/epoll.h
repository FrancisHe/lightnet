#pragma once
//  poller.h decides which polling mechanism to use.
#include "poller.h"
#if defined POLLER_USE_EPOLL
#include <sys/epoll.h>
#include <unordered_map>
#include <memory>
#include "base_poller.h"

namespace LNETNS {
namespace event {

struct EpollOption {
  int max_events{256};  // max events 1 poll
};

// This class implements socket polling mechanism using the Linux-specific epoll mechanism.
class Epoll final : public BasePoller {
public:
  Epoll();
  Epoll(const EpollOption& opt);
  ~Epoll() override;

  bool UpsertFd(int fd, EventHandler* handler) override;
  bool UpsertFd(int fd, EventHandler* handler, int mask) override;
  bool RemoveFd(int fd) override;
  bool UpdateFdEvents(int fd, int mask) override;

  bool SetEventIn(int fd) override;
  bool ResetEventIn(int fd) override;
  bool SetEventOut(int fd) override;
  bool ResetEventOut(int fd) override;

  int DoPoll() override;

  uint32_t FdCount() const override { return fd_table_.size(); }

private:
  struct EpollFdEntry {
    uint32_t events;  // epoll_events.events
    EventHandler* handler_{nullptr};
  };
  using FdTable = std::unordered_map<int, EpollFdEntry>;

  //  Main epoll file descriptor
  int epoll_fd_{BAD_FD};
  FdTable fd_table_;

  std::unique_ptr<epoll_event[]> fired_events_{nullptr};
  const EpollOption* option_{nullptr};

  static const EpollOption kDefaultOption;
  NON_COPYABLE_NOR_MOVABLE(Epoll)
};
using Poller = Epoll;

}  // namespace event
}  // namespace LNETNS

#endif  // POLLER_USE_EPOLL
