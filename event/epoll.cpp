#include "epoll.h"
#if defined POLLER_USE_EPOLL
#include <unistd.h>
#include <errno.h>
#include <new>

namespace LNETNS {
namespace event {

const EpollOption Epoll::kDefaultOption;

Epoll::Epoll() : Epoll(kDefaultOption) {
}

Epoll::Epoll(const EpollOption& opt) {
#ifdef POLLER_USE_EPOLL_CLOEXEC
  // Setting this option result in sane behaviour when exec() functions are used.
  // Old sockets are closed and don't block TCP ports, avoid leaks, etc.
  epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
#else
  // Since Linux 2.6.8, the size argument is ignored, but must be greater than zero.
  epoll_fd_ = epoll_create(1024);
#endif
  bad_ = epoll_fd_ == -1;
  if (bad_) {
    errno_ = errno;
    return;
  }

  if (&opt != &kDefaultOption) {
    option_ = new EpollOption(opt);
  } else {
    option_ = &opt;
  }
  fired_events_.reset(new epoll_event[option_->max_events]);
}

Epoll::~Epoll() {
  if (epoll_fd_ != BAD_FD) {
    close(epoll_fd_);
    epoll_fd_ = BAD_FD;
  }
  if (option_ != &kDefaultOption) {
    delete option_;
    option_ = nullptr;
  }
}

bool Epoll::UpsertFd(int fd, EventHandler* handler) {
  return UpsertFd(fd, handler, 0);
}

bool Epoll::UpsertFd(int fd, EventHandler* handler, int mask) {
  if (bad_ || fd < 0 || !handler) {
    return false;
  }

  epoll_event ev;
  ev.events = 0;
  ev.data.fd = fd;
  if (mask & kEventIn) {
    ev.events |= EPOLLIN;
  }
  if (mask & kEventOut) {
    ev.events |= EPOLLOUT;
  }

  int rc = 0;
  auto iter = fd_table_.find(fd);
  if (iter == fd_table_.end()) {
    rc = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);
  } else {
    rc = epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
  }
  if (rc != 0) {
    errno_ = errno;
    return false;
  }

  if (iter == fd_table_.end()) {
    fd_table_.emplace(fd, EpollFdEntry{ev.events, handler});
  } else {
    iter->second.events = ev.events;
    iter->second.handler_ = handler;
  }

  return true;
}

bool Epoll::UpdateFdEvents(int fd, int mask) {
  if (bad_ || fd < 0) {
    return false;
  }

  auto iter = fd_table_.find(fd);
  if (iter == fd_table_.end()) {
    return false;
  }

  epoll_event ev;
  ev.events = iter->second.events;
  ev.data.fd = fd;
#ifdef NO_ZERO_EVENT
  if (!mask) {
    // No interesting event.
    fd_table_.erase(iter);
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, &ev);
    return true;
  }
#endif  // NO_ZERO_EVENT

  if (mask & kEventIn) {
    ev.events |= EPOLLIN;
  } else {
    ev.events &= ~(static_cast<uint32_t>(EPOLLIN));
  }
  if (mask & kEventOut) {
    ev.events |= EPOLLOUT;
  } else {
    ev.events &= ~(static_cast<uint32_t>(EPOLLOUT));
  }
  int rc = epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
  if (rc != 0) {
    errno_ = errno;
    return false;
  }

  iter->second.events = ev.events;
  return true;
}

bool Epoll::RemoveFd(int fd) {
  if (bad_ || fd < 0) {
    return false;
  }

  // erase() returns number of elements removed (0 or 1).
  bool deleted = fd_table_.erase(fd);  

  // Before Linux 2.6.9, the EPOLL_CTL_DEL operation required a non-null
  // pointer in event, even though this argument is ignored.
  // Since Linux 2.6.9, event can be specified as NULL when using EPOLL_CTL_DEL.
  epoll_event ev;
  ev.events = 0;
  ev.data.fd = fd;
  int rc = epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, &ev);
  if (rc != 0) {
    return false;
  }

  return deleted;
}

bool Epoll::SetEventIn(int fd) {
  if (bad_ || fd < 0) {
    return false;
  }

  auto iter = fd_table_.find(fd);
  if (iter == fd_table_.end()) {
    return false;
  }

  epoll_event ev;
  ev.events = iter->second.events;
  ev.data.fd = fd;
  ev.events |= EPOLLIN;
  int rc = epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
  if (rc != 0) {
    errno_ = errno;
    return false;
  }

  iter->second.events = ev.events;
  return true;
}

bool Epoll::ResetEventIn(int fd) {
  if (bad_ || fd < 0) {
    return false;
  }

  auto iter = fd_table_.find(fd);
  if (iter == fd_table_.end()) {
    return false;
  }

  epoll_event ev;
  ev.events = iter->second.events;
  ev.data.fd = fd;
  ev.events &= ~(static_cast<uint32_t>(EPOLLIN));
#ifdef NO_ZERO_EVENT
  if (!ev.events) {
    // No interesting event.
    fd_table_.erase(iter);
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, &ev);
    return true;
  }
#endif  // NO_ZERO_EVENT

  int rc = epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
  if (rc != 0) {
    errno_ = errno;
    return false;
  }

  iter->second.events = ev.events;
  return true;
}

bool Epoll::SetEventOut(int fd) {
  if (bad_ || fd < 0) {
    return false;
  }

  auto iter = fd_table_.find(fd);
  if (iter == fd_table_.end()) {
    return false;
  }

  epoll_event ev;
  ev.events = iter->second.events;
  ev.data.fd = fd;
  ev.events |= EPOLLOUT;
  int rc = epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
  if (rc != 0) {
    errno_ = errno;
    return false;
  }

  iter->second.events = ev.events;
  return true;
}

bool Epoll::ResetEventOut(int fd) {
  if (bad_ || fd < 0) {
    return false;
  }

  auto iter = fd_table_.find(fd);
  if (iter == fd_table_.end()) {
    return false;
  }

  epoll_event ev;
  ev.events = iter->second.events;
  ev.data.fd = fd;
  ev.events &= ~(static_cast<uint32_t>(EPOLLOUT));
#ifdef NO_ZERO_EVENT
  if (!ev.events) {
    // No interesting event.
    fd_table_.erase(iter);
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, &ev);
    return true;
  }
#endif  // NO_ZERO_EVENT

  int rc = epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
  if (rc != 0) {
    errno_ = errno;
    return false;
  }

  iter->second.events = ev.events;
  return true;
}

int Epoll::DoPoll() {
  if (bad_) {
    return -1;
  }
  errno_ = 0;

  // 0 means there is due timer, -1 means there is no timer.
  int timeout = EarliestTimeout();
  // Avoid waiting infinitely.
  if (fd_table_.empty() && timeout < 0) {
    return 0;
  }

  // If FdTable is empty and timeout > 0, DoPoll() act as sleep.
  // 
  // timeout = 0 - return immediately;
  // timeout > 0 - waiting for timeout milliseconds;
  // timeout = -1 - infinitely wait (must have fd to listen).
  int n = epoll_wait(epoll_fd_, fired_events_.get(), option_->max_events, timeout);
  if (n == -1) {
    // TODO: try again if errno is EINTR (a signal was caught)?
    errno_ = errno;  // https://en.cppreference.com/w/cpp/error/errno
    return -1;
  }

  int nevents = 0;
  for (int i = 0; i < n; ++i) {
    auto& ev = fired_events_[i];
    if (ev.events & EPOLLIN) {
      auto entry_iter = fd_table_.find(ev.data.fd);
      if (entry_iter == fd_table_.end()) {
        continue;
      }
      entry_iter->second.handler_->OnReadable(ev.data.fd);
      ++nevents;
    }

    if (ev.events & EPOLLOUT) {
      auto entry_iter = fd_table_.find(ev.data.fd);
      if (entry_iter == fd_table_.end()) {
        continue;
      }
      entry_iter->second.handler_->OnWritable(ev.data.fd);
      ++nevents;
    }
    
    if (ev.events & (EPOLLERR | EPOLLHUP)) {
      auto entry_iter = fd_table_.find(ev.data.fd);
      if (entry_iter == fd_table_.end()) {
        continue;
      }
      entry_iter->second.handler_->OnError(ev.data.fd);
      ++nevents;
    }
  }

  return nevents + ProcessTimeEvents();
}

}  // namespace event
}  // namespace LNETNS

#endif  // POLLER_USE_EPOLL
