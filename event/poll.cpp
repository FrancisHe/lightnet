#include "poll.h"  // POSIX poll() system call is in header <poll.h>
#if defined POLLER_USE_POLL
#include <algorithm>

namespace net {
namespace event {

const PollOption Poll::kDefaultOption;

Poll::Poll() : Poll(kDefaultOption) {
}

Poll::Poll(const PollOption& opt) {
  if (&opt != &kDefaultOption) {
    option_ = new PollOption(opt);
  } else {
    option_ = &opt;
  }
}

Poll::~Poll() {
  if (option_ != &kDefaultOption) {
    delete option_;
    option_ = nullptr;
  }
}

bool Poll::UpsertFd(int fd, EventHandler* handler) {
  return UpsertFd(fd, handler, 0);
}

bool Poll::UpsertFd(int fd, EventHandler* handler, int mask) {
  if (fd < 0 || !handler) {
    return false;
  }

  short events = 0;
  if (mask & kEventIn) {
    events |= POLLIN;
  }
  if (mask & kEventOut) {
    events |= POLLOUT;
  }

  auto iter = fd_table_.find(fd);
  if (iter == fd_table_.end()) {
    // Note: after emplace_back, a reallocation may take place, in which case
    // all iterators and all references to the elements are invalidated.
    poll_set_.emplace_back(pollfd{fd, events, 0});
    PollFdEntry fd_entry;
    fd_entry.index_ = poll_set_.size() - 1;
    fd_entry.handler_ = handler;
    fd_table_.emplace(fd, std::move(fd_entry));
  } else {
    poll_set_[iter->second.index_].events = events;
    iter->second.handler_ = handler;
  }

  return true;
}

bool Poll::UpdateFdEvents(int fd, int mask) {
  if (fd < 0) {
    return false;
  }

  auto entry_iter = fd_table_.find(fd);
  if (entry_iter == fd_table_.end()) {
    return false;
  }

  auto& pfd = poll_set_[entry_iter->second.index_];
#ifdef NO_ZERO_EVENT
  if (!mask) {
    // No interesting event.
    pfd.fd = BAD_FD;
    fd_table_.erase(entry_iter);
    ++retired_fd_cnt_;
    return true;
  }
#endif  // NO_ZERO_EVENT

  if (mask & kEventIn) {
    pfd.events |= POLLIN;
  } else {
    pfd.events &= ~(static_cast<short>(POLLIN));
  }
  if (mask & kEventOut) {
    pfd.events |= POLLOUT;
  } else {
    pfd.events &= ~(static_cast<short>(POLLOUT));
  }

  return true;
}

bool Poll::RemoveFd(int fd) {
  if (fd < 0) {
    return false;
  }

  auto entry_iter = fd_table_.find(fd);
  if (entry_iter == fd_table_.end()) {
    return false;
  }

  auto& pfd = poll_set_[entry_iter->second.index_];
  pfd.fd = BAD_FD;  // remove in ShrinkPollSet()
  fd_table_.erase(entry_iter);
  ++retired_fd_cnt_;

  return true;
}

bool Poll::SetEventIn(int fd) {
  if (fd < 0) {
    return false;
  }

  auto entry_iter = fd_table_.find(fd);
  if (entry_iter == fd_table_.end()) {
    return false;
  }

  auto& pfd = poll_set_[entry_iter->second.index_];
  pfd.events |= POLLIN;

  return true;
}

bool Poll::ResetEventIn(int fd) {
  if (fd < 0) {
    return false;
  }

  auto entry_iter = fd_table_.find(fd);
  if (entry_iter == fd_table_.end()) {
    return false;
  }

  auto& pfd = poll_set_[entry_iter->second.index_];
  pfd.events &= ~(static_cast<short>(POLLIN));
#ifdef NO_ZERO_EVENT
  if (!pfd.events) {
    // No interesting event.
    pfd.fd = BAD_FD;
    fd_table_.erase(entry_iter);
    ++retired_fd_cnt_;
  }
#endif  // NO_ZERO_EVENT

  return true;
}

bool Poll::SetEventOut(int fd) {
  if (fd < 0) {
    return false;
  }

  auto entry_iter = fd_table_.find(fd);
  if (entry_iter == fd_table_.end()) {
    return false;
  }

  auto& pfd = poll_set_[entry_iter->second.index_];
  pfd.events |= POLLOUT;

  return true;
}

bool Poll::ResetEventOut(int fd) {
  if (fd < 0) {
    return false;
  }

  auto entry_iter = fd_table_.find(fd);
  if (entry_iter == fd_table_.end()) {
    return false;
  }

  auto& pfd = poll_set_[entry_iter->second.index_];
  pfd.events &= ~(static_cast<short>(POLLOUT));
#ifdef NO_ZERO_EVENT
  if (!pfd.events) {
    // No interesting event.
    pfd.fd = BAD_FD;
    fd_table_.erase(entry_iter);
    ++retired_fd_cnt_;
  }
#endif  // NO_ZERO_EVENT

  return true;
}

int Poll::DoPoll() {
  errno_ = 0;

  // 0 means there is due timer, -1 means there is no timer.
  int timeout = EarliestTimeout();
  // Avoid waiting infinitely.
  if (fd_table_.empty() && timeout < 0) {
    return 0;
  }

  ShrinkPollSet();
  // From cppreference (https://en.cppreference.com/w/cpp/container/vector):
  // The elements are stored contiguously, which means that elements can be
  // accessed not only through iterators, but also using offsets to regular
  // pointers to elements. This means that a pointer to an element of a vector
  // may be passed to any function that expects a pointer to an element of an array.
  //
  // std::vector::data: returns pointer to the underlying array.
  //
  // Empty sets (nfds zero):
  // poll act as sleep when run with empty sets, see:
  // Curl_wait_ms - https://github.com/curl/curl/blob/master/lib/select.c
  //
  // timeout = 0 - return immediately;
  // timeout > 0 - waiting for timeout milliseconds;
  // timeout = -1 - infinitely wait (must have fd to listen).
  int rc = poll(poll_set_.data(), poll_set_.size(), timeout);
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
  // Note: calls to OnReadable/OnWritable/OnError may modify the FdTable and PollSet.
  //
  // The size of PollSet may increase after call to OnReadable/OnWritable/OnError,
  // in which case, any iterators/pointers/references may become invalid.
  // Fortunately, the index is still correct since we only append elements to the vector.
  // Thus, always use "poll_set_[i]" instead of saved references.
  nfds_t nfds = poll_set_.size();
  for (nfds_t i = 0; i < nfds; ++i) {  // use nfds instead of poll_set_.size()
    if (poll_set_[i].fd == BAD_FD) {
      continue;
    }
    if (poll_set_[i].revents & POLLIN) {
      auto entry_iter = fd_table_.find(poll_set_[i].fd);
      if (entry_iter == fd_table_.end()) {
        continue;
      }
      entry_iter->second.handler_->OnReadable(poll_set_[i].fd);
      ++nevents;
    }

    // Note: fd may have been closed after call to OnReadable.
    if (poll_set_[i].fd == BAD_FD) {
      continue;
    }
    if (poll_set_[i].revents & POLLOUT) {
      auto entry_iter = fd_table_.find(poll_set_[i].fd);
      if (entry_iter == fd_table_.end()) {
        continue;
      }
      entry_iter->second.handler_->OnWritable(poll_set_[i].fd);
      ++nevents;
    }

    if (poll_set_[i].fd == BAD_FD) {
      continue;
    }
    if (poll_set_[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
      auto entry_iter = fd_table_.find(poll_set_[i].fd);
      if (entry_iter == fd_table_.end()) {
        continue;
      }
      entry_iter->second.handler_->OnError(poll_set_[i].fd);
      ++nevents;
    }
  }

  return nevents + ProcessTimeEvents();
}

void Poll::ShrinkPollSet() {
  // To avoid modify the PollSet too frequently, we shrink the PollSet only
  // when both the total size and retired size reach the give values.
  // Negative fd in `pollfd` will be ignored by `poll()`. (Linux manual page)
  if (poll_set_.size() > option_->shrink_fd_cnt &&
      retired_fd_cnt_ > option_->shrink_retired_fd_cnt) {
    // https://cplusplus.com/reference/algorithm/remove_if/
    PollSet::size_type first = 0;
    for (PollSet::size_type i = 0; i < poll_set_.size(); ++i) {
      if (poll_set_[i].fd != BAD_FD) {
        if (first != i) {
          poll_set_[first] = poll_set_[i];
          fd_table_[poll_set_[i].fd].index_ = first;
        }
        ++first;
      }
    }

    // std::vector::resize: If the current size is greater than `count`,
    // the container is reduced to its first `count` elements.
    poll_set_.resize(first);
    retired_fd_cnt_ = 0;
  }
}

}  // namespace event
}  // namespace net

#endif  // POLLER_USE_POLL
