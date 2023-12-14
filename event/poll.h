#pragma once
//  poller.h decides which polling mechanism to use.
#include "poller.h"
#if defined POLLER_USE_POLL
#include <poll.h>
#include <unordered_map>
#include <vector>
#include "base_poller.h"

namespace net {
namespace event {

struct PollOption {
  uint32_t shrink_fd_cnt{4096};
  uint32_t shrink_retired_fd_cnt{512};
};

// Implements socket polling mechanism using the POSIX.1-2001 poll() system call.
// https://pubs.opengroup.org/onlinepubs/9699919799/functions/poll.html
class Poll final : public BasePoller {
public:
  Poll();
  Poll(const PollOption& opt);
  ~Poll() override;

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
  void ShrinkPollSet();

private:
  using PollSet = std::vector<pollfd>;

  struct PollFdEntry {
    PollSet::size_type index_;  // PollSet index
    EventHandler* handler_{nullptr};
  };

  //  This table stores data for registered descriptors.
  using FdTable = std::unordered_map<int, PollFdEntry>;
  FdTable fd_table_;

  //  Poll set to pass to the poll function.
  PollSet poll_set_;
  uint32_t retired_fd_cnt_{0};
  const PollOption* option_{nullptr};

  static const PollOption kDefaultOption;
  NON_COPYABLE_NOR_MOVABLE(Poll)
};
using Poller = Poll;

}  // namespace event
}  // namespace net

#endif  // POLLER_USE_POLL
