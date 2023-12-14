#include "base_poller.h"
#include <algorithm>

namespace net {
namespace event {

TimerKey BasePoller::AddTimer(uint32_t timeout, EventHandler* handler, int id) {
  uint64_t expiration = GetNowMs() + timeout;
  auto& lst = timers_[expiration];
  auto data_it = std::find_if(lst.begin(), lst.end(), [&](const TimerData& d)
                              { return d.handler == handler && d.id == id; });
  if (data_it == lst.end()) {
    lst.emplace_back(handler, id);
    return expiration;
  } else {
    // Exists.
    return kBadTimerKey;
  }
}

bool BasePoller::CancelTimer(TimerKey expiration, EventHandler* handler, int id) {
  auto lst_it = timers_.find(expiration);
  if (lst_it == timers_.end()) {
    return false;
  }

  auto& lst = lst_it->second;
  auto data_it = std::find_if(lst.begin(), lst.end(), [&](const TimerData& d)
                              { return d.handler == handler && d.id == id; });
  if (data_it == lst.end()) {
    return false;
  }

  lst.erase(data_it);
  if (lst.empty()) {
    timers_.erase(lst_it);
  }

  return true;
}

int BasePoller::EarliestTimeout() {
  if (timers_.empty()) {
    return -1;
  }

  auto earliest = timers_.begin()->first;
  auto now = GetNowMs();
  return earliest > now ? earliest - now : 0;
}

int BasePoller::ProcessTimeEvents() {
  auto now = GetNowMs();
  std::list<TimerData> fired_timers;
  for (auto lst_it = timers_.begin(); lst_it != timers_.end();) {
    auto expiration = lst_it->first;
    if (expiration > now) {
      // There is no need to check the subsequent timers since std::map is sorted.
      break;
    }

    // Note: we don't call handler->OnTimeout() in the loop since it may modify timers_.
    fired_timers.splice(fired_timers.end(), lst_it->second);
    timers_.erase(lst_it++);
  }

  for (auto& timer : fired_timers) {
    // handler can be null.
    if (timer.handler) {
      timer.handler->OnTimeout(timer.id);
    }
  }

  return fired_timers.size();
}

uint64_t BasePoller::GetNowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    MonotonicClock::now().time_since_epoch()).count();
}

}  // namespace event
}  // namespace net
