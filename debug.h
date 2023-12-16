#pragma once
#ifdef LNET_DEBUG
#include "fmt/format.h"

#define LOG_ERROR(format, args...) fmt::println(stderr, __FILE__":{}:ERROR: " format, __LINE__, ##args)
#define LOG_WARN(format, args...) fmt::println(__FILE__":{}:WARN: " format, __LINE__, ##args)
#define LOG_INFO(format, args...) fmt::println(__FILE__":{}:INFO: " format, __LINE__, ##args)
#define LOG_DEBUG(format, args...) fmt::println(__FILE__":{}:DEBUG: " format, __LINE__, ##args)

#else

// "no-op" macro:
// https://stackoverflow.com/questions/1306611/how-do-i-implement-no-op-macro-or-template-in-c
// curl use "do {} while(0)"
#define LOG_ERROR(format, args...) do {} while(0)
#define LOG_WARN(format, args...) do {} while(0)
#define LOG_INFO(format, args...) do {} while(0)
#define LOG_DEBUG(format, args...) do {} while(0)

#endif  // LNET_DEBUG
