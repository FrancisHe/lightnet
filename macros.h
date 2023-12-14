#pragma once
#include "config.h"

#define BAD_FD -1

#if !defined NON_COPYABLE_NOR_MOVABLE
#define NON_COPYABLE_NOR_MOVABLE(ClassName)         \
public:                                             \
  ClassName(const ClassName &) = delete;            \
  ClassName &operator=(const ClassName &) = delete; \
  ClassName(ClassName &&) = delete;                 \
  ClassName &operator=(ClassName &&) = delete;
#endif
