// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_LAZY_NOW_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_LAZY_NOW_H_

#include "base/optional.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace base {
class TickClock;
}

namespace blink {
namespace scheduler {

// Now() is somewhat expensive so it makes sense not to call Now() unless we
// really need to and to avoid subsequent calls if already called once.
// LazyNow objects are expected to be short-living to represent accurate time.
class PLATFORM_EXPORT LazyNow {
 public:
  explicit LazyNow(base::TimeTicks now);
  explicit LazyNow(const base::TickClock* tick_clock);

  LazyNow(LazyNow&& move_from);

  // Result will not be updated on any subsesequent calls.
  base::TimeTicks Now();

 private:
  const base::TickClock* tick_clock_;  // Not owned.
  base::TimeTicks now_;

  DISALLOW_COPY_AND_ASSIGN(LazyNow);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_LAZY_NOW_H_
