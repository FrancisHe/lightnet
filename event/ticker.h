#pragma once
#include "poller.h"

namespace LNETNS {
namespace event {

// Periodically triggered timer.
class Ticker : public EventHandler {
public:
  Ticker(Poller* poller, uint32_t interval);
  Ticker() = delete;
  ~Ticker() override;

  bool Start();
  void Stop();
  inline bool Stopped() const {
    return timer_key_ == kBadTimerKey;
  }

protected:
  // You can stop the timer in OnFire().
  virtual void OnFire() {}

  // You can override OnReadable() and OnWritable().
  void OnReadable(int fd) override {}
  void OnWritable(int fd) override {}

private:
  void OnTimeout(int id) override;

private:
  Poller* poller_{nullptr};
  uint32_t interval_{0};
  TimerKey timer_key_{kBadTimerKey};
};

}  // namespace event
}  // namespace LNETNS
