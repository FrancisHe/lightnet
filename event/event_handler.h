#pragma once
#include "config.h"

namespace LNETNS {
namespace event {

enum EventType {
  kEventIn = 1,
  kEventOut = 2,
  kEventError = 4,  // output only
};

class EventHandler {
public:
  virtual ~EventHandler() = default;

  // Called when file descriptor is ready for reading/writing.
  // A handler may have multiple file descriptors, poller should send it back.
  virtual void OnReadable(int fd) = 0;
  virtual void OnWritable(int fd) = 0;
  virtual void OnError(int fd) {}

  // Called when timer expires.
  // A handler may have multiple timers, use id to identify them.
  //
  // If the timer is just used to set timeout for poller, don't override it.
  virtual void OnTimeout(int id) {}

protected:
  // Set constructor protected to make the base class not instantiable.
  EventHandler() = default;
};

}  // namespace event
}  // namespace LNETNS
