// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EventDispatchForbiddenScope_h
#define EventDispatchForbiddenScope_h

#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/AutoReset.h"

namespace blink {

#if DCHECK_IS_ON()

class EventDispatchForbiddenScope {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(EventDispatchForbiddenScope);

 public:
  EventDispatchForbiddenScope() {
    ASSERT(IsMainThread());
    ++count_;
  }

  ~EventDispatchForbiddenScope() {
    ASSERT(IsMainThread());
    ASSERT(count_);
    --count_;
  }

  static bool IsEventDispatchForbidden() {
    if (!IsMainThread())
      return false;
    return count_;
  }

  class AllowUserAgentEvents {
    STACK_ALLOCATED();

   public:
    AllowUserAgentEvents() : change_(&count_, 0) { ASSERT(IsMainThread()); }

    ~AllowUserAgentEvents() { ASSERT(!count_); }

    AutoReset<unsigned> change_;
  };

 private:
  PLATFORM_EXPORT static unsigned count_;
};

#else

class EventDispatchForbiddenScope {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(EventDispatchForbiddenScope);

 public:
  EventDispatchForbiddenScope() {}

  class AllowUserAgentEvents {
   public:
    AllowUserAgentEvents() {}
  };
};

#endif  // DCHECK_IS_ON()

}  // namespace blink

#endif  // EventDispatchForbiddenScope_h
