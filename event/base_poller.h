#pragma once
#include <chrono>
#include <map>
#include <list>
#include "macros.h"
#include "event_handler.h"

namespace net {
namespace event {

using TimerKey = uint64_t;
static constexpr TimerKey kBadTimerKey = 0;
static constexpr int kDefaultTimerID = 0;

// Note: user should call RemoveFd() or CancelTimer() before release EventHandler.
// If "handler" has been released but fd or timer is still there, the saved "handler"
// pointer in poller becomes dangling, which will cause segfault when event occurs.
//
// A special case is that user just wants to set a timeout for the poller with doing
// nothing when timed out, just pass a null "handler" when AddTimer.
class BasePoller {
public:
  BasePoller() = default;
  virtual ~BasePoller() = default;

  // Check if the constructor succeeded using the following functions
  // since C++ constructor has no return value.
  inline bool Bad() const { return bad_; }
  inline operator bool() const { return !bad_; }
  inline bool operator!() const { return bad_; }

  // The "upsert" term means "update" or "insert".
  // https://english.stackexchange.com/questions/227915/word-meaning-both-create-and-update
  virtual bool UpsertFd(int fd, EventHandler* handler) = 0;
  virtual bool UpsertFd(int fd, EventHandler* handler, int mask) = 0;
  virtual bool RemoveFd(int fd) = 0;
  virtual bool UpdateFdEvents(int fd, int mask) = 0;

  virtual bool SetEventIn(int fd) = 0;
  virtual bool ResetEventIn(int fd) = 0;
  virtual bool SetEventOut(int fd) = 0;
  virtual bool ResetEventOut(int fd) = 0;

  // One-time timer, handler->OnTimeout() will be called when timeout milliseconds elapsed.
  // Note that a handler may has multiple timers, use id to identify them.
  //
  // Return monotonic expiration time as key.
  TimerKey AddTimer(uint32_t timeout, EventHandler* handler, int id);
  // Cancel timer with key returned by AddTimer().
  //
  // Note: be sure all timers have been cancelled or fired before handler being released.
  bool CancelTimer(TimerKey key, EventHandler* handler, int id);

  // Ignore "id" (use kDefaultTimerID).
  inline TimerKey AddTimer(uint32_t timeout, EventHandler* handler) {
    return AddTimer(timeout, handler, kDefaultTimerID);
  }
  inline bool CancelTimer(TimerKey key, EventHandler* handler) {
    return CancelTimer(key, handler, kDefaultTimerID);
  }

  virtual int DoPoll() = 0;

  virtual uint32_t FdCount() const = 0;
  inline uint32_t TimerCount() const { return timers_.size(); }
  virtual int MaxFd() const { return BAD_FD; }

  inline int GetLastErrno() const { return errno_; }
  static uint64_t GetNowMs();

protected:
  struct TimerData {
    EventHandler* handler{nullptr};
    int id{0};

    TimerData(EventHandler* hdlr, int id) : handler(hdlr), id(id) {}
  };
  using TimerStore = std::map<uint64_t, std::list<TimerData> >;
  using MonotonicClock = std::chrono::steady_clock;

  // Returns number of milliseconds to wait to match the next timer or
  // 0 meaning "there is timed out timer" or -1 meaning "no timer".
  int EarliestTimeout();

  // Executes any timers that are due.
  // Returns the number of fired timers.
  int ProcessTimeEvents();

protected:
  bool bad_{false};
  int errno_{0};
  TimerStore timers_;
};

}  // namespace event
}  // namespace net
