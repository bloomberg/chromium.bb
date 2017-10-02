// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EventDispatchForbiddenScope_h
#define EventDispatchForbiddenScope_h

#include "base/macros.h"
#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/AutoReset.h"

namespace blink {

#if DCHECK_IS_ON()

class EventDispatchForbiddenScope {
  STACK_ALLOCATED();
  DISALLOW_COPY_AND_ASSIGN(EventDispatchForbiddenScope);

 public:
  EventDispatchForbiddenScope() {
    DCHECK(IsMainThread());
    ++count_;
  }

  ~EventDispatchForbiddenScope() {
    DCHECK(IsMainThread());
    DCHECK(count_);
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
    AllowUserAgentEvents() : change_(&count_, 0) { DCHECK(IsMainThread()); }

    ~AllowUserAgentEvents() { DCHECK(!count_); }

    AutoReset<unsigned> change_;
  };

 private:
  PLATFORM_EXPORT static unsigned count_;
};

#else

class EventDispatchForbiddenScope {
  STACK_ALLOCATED();
  DISALLOW_COPY_AND_ASSIGN(EventDispatchForbiddenScope);

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
