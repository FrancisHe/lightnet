#include "ticker.h"
#include <cassert>

namespace LNETNS {
namespace event {

Ticker::Ticker(Poller* poller, uint32_t interval)
  : poller_(poller), interval_(interval) {
}

Ticker::~Ticker() {
  Stop();
}

bool Ticker::Start() {
  if (interval_ == 0) {
    return false;
  }

  if (timer_key_ != kBadTimerKey) {
    return false;
  }

  assert(poller_);
  timer_key_ = poller_->AddTimer(interval_, this);
  return timer_key_ != kBadTimerKey;
}

void Ticker::Stop() {
  assert(poller_);
  if (timer_key_ != kBadTimerKey) {
    poller_->CancelTimer(timer_key_, this);
    timer_key_ = kBadTimerKey;
  }
}

void Ticker::OnTimeout(int id) {
  OnFire();
  assert(poller_);
  if (!Stopped()) {
    timer_key_ = poller_->AddTimer(interval_, this);
  }
}

}  // namespace event
}  // namespace LNETNS
