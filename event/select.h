#pragma once
//  poller.h decides which polling mechanism to use.
#include "poller.h"
#if defined POLLER_USE_SELECT
#include <sys/select.h>
#include <map>
#include <string>
#include <vector>
#include "base_poller.h"

namespace LNETNS {
namespace event {

// Implements socket polling mechanism using POSIX.1-2001 select() function.
// https://pubs.opengroup.org/onlinepubs/9699919799/functions/select.html
class Select final : public BasePoller {
public:
  Select();
  ~Select() override;

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
  int MaxFd() const override { return FD_SETSIZE; }

private:
  void TriggerFdEvents(std::vector<std::pair<int, int> >& fired_fds);

private:
  using FdTable = std::map<int, EventHandler*>;

  struct FdSet {
    inline void RemoveFd(int fd) {
      FD_CLR(fd, &read);
      FD_CLR(fd, &write);
      FD_CLR(fd, &error);
    }

    fd_set read;
    fd_set write;
    fd_set error;
  };

  FdTable fd_table_;
  FdSet fd_set_;

  NON_COPYABLE_NOR_MOVABLE(Select)
};
using Poller = Select;

}  // namespace event
}  // namespace LNETNS

#endif  // POLLER_USE_SELECT
