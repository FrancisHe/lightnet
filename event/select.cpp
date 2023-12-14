#include "select.h"  // POSIX select() system call is in header <sys/select.h>
#if defined POLLER_USE_SELECT

namespace net {
namespace event {

Select::Select() {
  FD_ZERO(&fd_set_.read);
  FD_ZERO(&fd_set_.write);
  FD_ZERO(&fd_set_.error);
}

Select::~Select() {
}

bool Select::UpsertFd(int fd, EventHandler* handler) {
  return UpsertFd(fd, handler, 0);
}

bool Select::UpsertFd(int fd, EventHandler* handler, int mask) {
  if (fd < 0 || fd > FD_SETSIZE || !handler) {
    return false;
  }

  auto iter = fd_table_.find(fd);
  if (iter == fd_table_.end()) {
    fd_table_.emplace(fd, handler);
  } else {
    iter->second = handler;
  }

  if (mask & kEventIn) {
    FD_SET(fd, &fd_set_.read);
  }
  if (mask & kEventOut) {
    FD_SET(fd, &fd_set_.write);
  }
  FD_SET(fd, &fd_set_.error);

  return true;
}

bool Select::UpdateFdEvents(int fd, int mask) {
  if (fd < 0 || fd > FD_SETSIZE) {
    return false;
  }
  auto entry_iter = fd_table_.find(fd);
  if (entry_iter == fd_table_.end()) {
    return false;
  }

#ifdef NO_ZERO_EVENT
  if (!mask) {
    // No interesting event.
    fd_table_.erase(entry_iter);
    fd_set_.RemoveFd(fd);
    return true;
  }
#endif  // NO_ZERO_EVENT

  if (mask & kEventIn) {
    FD_SET(fd, &fd_set_.read);
  } else {
    FD_CLR(fd, &fd_set_.read);
  }
  if (mask & kEventOut) {
    FD_SET(fd, &fd_set_.write);
  } else {
    FD_CLR(fd, &fd_set_.write);
  }

  return true;
}

bool Select::RemoveFd(int fd) {
  if (fd < 0 || fd > FD_SETSIZE) {
    return false;
  }

  fd_set_.RemoveFd(fd);
  // erase() returns number of elements removed (0 or 1).
  return fd_table_.erase(fd);
}

bool Select::SetEventIn(int fd) {
  if (fd < 0 || fd > FD_SETSIZE) {
    return false;
  }
  if (!fd_table_.count(fd)) {
    return false;
  }

  FD_SET(fd, &fd_set_.read);
  return true;
}

bool Select::ResetEventIn(int fd) {
  if (fd < 0 || fd > FD_SETSIZE) {
    return false;
  }
  auto entry_iter = fd_table_.find(fd);
  if (entry_iter == fd_table_.end()) {
    return false;
  }

  FD_CLR(fd, &fd_set_.read);
#ifdef NO_ZERO_EVENT
  if (!FD_ISSET(fd, &fd_set_.write)) {
    // No interesting event.
    fd_table_.erase(entry_iter);
    FD_CLR(fd, &fd_set_.error);
  }
#endif  // NO_ZERO_EVENT

  return true;
}

bool Select::SetEventOut(int fd) {
  if (fd < 0 || fd > FD_SETSIZE) {
    return false;
  }
  if (!fd_table_.count(fd)) {
    return false;
  }

  FD_SET(fd, &fd_set_.write);
  return true;
}

bool Select::ResetEventOut(int fd) {
  if (fd < 0 || fd > FD_SETSIZE) {
    return false;
  }
  auto entry_iter = fd_table_.find(fd);
  if (entry_iter == fd_table_.end()) {
    return false;
  }

  FD_CLR(fd, &fd_set_.write);
#ifdef NO_ZERO_EVENT
  if (!FD_ISSET(fd, &fd_set_.read)) {
    // No interesting event.
    fd_table_.erase(entry_iter);
    FD_CLR(fd, &fd_set_.error);
  }
#endif  // NO_ZERO_EVENT

  return true;
}

int Select::DoPoll() {
  errno_ = 0;

  // 0 means there is due timer, -1 means there is no timer.
  int timeout = EarliestTimeout();
  // Avoid waiting infinitely.
  if (fd_table_.empty() && timeout < 0) {
    return 0;
  }

  // std::map is sorted by key, so the last key in fd_entries_ is maximum.
  int max_fd = -1;
  if (!fd_table_.empty()) {
    max_fd = fd_table_.rbegin()->first;
  }

  // Note that select() will change the input fd_set, so we should pass a copy.
  FdSet tmp_fd_set = fd_set_;
  timeval tv;
  if (timeout >= 0) {
    tv = {static_cast<long>(timeout / 1000), static_cast<long>(timeout % 1000 * 1000)};
  }
  // Empty sets (nfds zero):
  // select act as sleep when run with empty sets, see:
  // https://man7.org/linux/man-pages/man2/select.2.html
  //
  // other references:
  // Curl_wait_ms - https://github.com/curl/curl/blob/master/lib/select.c
  // curl_multi_wait - https://curl.se/libcurl/c/curl_multi_wait.html
  //
  // timeout = 0 - return immediately;
  // timeout > 0 - waiting for timeout milliseconds;
  // timeout = -1 - infinitely wait (must have fd to listen).
  int rc = select(max_fd + 1, &tmp_fd_set.read, &tmp_fd_set.write,
                  &tmp_fd_set.error, timeout >= 0 ? &tv : NULL);
  if (rc == -1) {
    // TODO: try again if errno is EINTR (a signal was caught)?
    errno_ = errno;  // https://en.cppreference.com/w/cpp/error/errno
    return -1;
  }
  if (rc == 0) {
    // No event occurs.
    return ProcessTimeEvents();
  }

  int nevents = 0;
  std::vector<std::pair<int, int> > fired_fds;
  for (auto& entry : fd_table_) {
    auto fd = entry.first;
    int fired_events = 0;
    if (FD_ISSET(fd, &tmp_fd_set.read)) {
      fired_events |= kEventIn;
      ++nevents;
    }
    if (FD_ISSET(fd, &tmp_fd_set.write)) {
      fired_events |= kEventOut;
      ++nevents;
    }
    if (FD_ISSET(fd, &tmp_fd_set.error)) {
      fired_events |= kEventError;
      ++nevents;
    }
    fired_fds.emplace_back(fd, fired_events);
  }
  TriggerFdEvents(fired_fds);

  return nevents + ProcessTimeEvents();
}

void Select::TriggerFdEvents(std::vector<std::pair<int, int> >& fired_fds) {
  // Note: calls to OnReadable/OnWritable/OnError may modify the FdTable.
  for (auto& fired : fired_fds) {
    auto fd = fired.first;
    auto fired_events = fired.second;
    if (fired_events & kEventIn) {
      auto entry_iter = fd_table_.find(fd);
      if (entry_iter == fd_table_.end()) {
        continue;
      }
      entry_iter->second->OnReadable(fd);
    }

    if (fired_events & kEventOut) {
      auto entry_iter = fd_table_.find(fd);
      if (entry_iter == fd_table_.end()) {
        continue;
      }
      entry_iter->second->OnWritable(fd);
    }

    if (fired_events & kEventError) {
      auto entry_iter = fd_table_.find(fd);
      if (entry_iter == fd_table_.end()) {
        continue;
      }
      entry_iter->second->OnError(fd);
    }
  }
}

}  // namespace event
}  // namespace net

#endif  // POLLER_USE_SELECT
