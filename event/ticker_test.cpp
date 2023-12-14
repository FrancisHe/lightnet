#define private public
#include "ticker.h"
#undef private
#include "gtest/gtest.h"

namespace LNETNS {
namespace event {
namespace test {

struct Counter : public Ticker {
  Counter(Poller* poller, uint32_t interval) : Ticker(poller, interval) {}
  Counter() = delete;

  void OnFire() {
    ++count_;
  }

  uint32_t count_{0};
};

struct StopWatch : public Ticker {
  StopWatch(Poller* poller, uint32_t interval, uint32_t times)
    : Ticker(poller, interval), times_(times) {}
  StopWatch() = delete;

  void OnFire() {
    --times_;
    if (!times_) {
      Stop();
    }
  }

  uint32_t times_{0};
};

}  // namespace test
}  // namespace event
}  // namespace LNETNS

#define TESTNS LNETNS::event::test

GTEST_TEST(TickerTest, BasicTest) {
  auto poller = std::make_unique<LNETNS::event::Poller>();
  auto ticker = std::make_unique<TESTNS::Counter>(poller.get(), 10);

  uint32_t count = 0;
  ticker->Start();
  do {
    poller->DoPoll();
  } while (++count < 10);
  ticker->Stop();

  EXPECT_EQ(ticker->count_, 10);
  EXPECT_EQ(ticker->timer_key_, LNETNS::event::kBadTimerKey);
  EXPECT_EQ(poller->FdCount(), 0);
  EXPECT_EQ(poller->TimerCount(), 0);
  EXPECT_EQ(poller->DoPoll(), 0);  // Should return immediately.

  // Release Ticker first.
  ticker.reset();  // release
  poller.reset();  // release
}

GTEST_TEST(TickerTest, AutoStopTest) {
  auto poller = std::make_unique<LNETNS::event::Poller>();
  auto stop_watch = std::make_unique<TESTNS::StopWatch>(poller.get(), 10, 5);

  uint32_t count = 0;
  stop_watch->Start();
  do {
    poller->DoPoll();
  } while (++count < 5);

  EXPECT_EQ(count, 5);
  EXPECT_TRUE(stop_watch->Stopped());
  EXPECT_EQ(stop_watch->timer_key_, LNETNS::event::kBadTimerKey);
  EXPECT_EQ(poller->TimerCount(), 0);

  // Release Ticker first.
  stop_watch.reset();  // release
  poller.reset();  // release
}

#undef TESTNS
