#pragma once
#include "macros.h"

#if defined POLLER_USE_EPOLL    \
    + defined POLLER_POLL       \
    + defined POLLER_USE_SELECT \
  > 1
#error More than one of the POLLER_USE_* macros defined
#endif

#if defined POLLER_USE_EPOLL
#include "epoll.h"
#elif defined POLLER_USE_POLL
#include "poll.h"
#elif defined POLLER_USE_SELECT
#include "select.h"
#else
#error None of the POLLER_USE_* macros defined
#endif
