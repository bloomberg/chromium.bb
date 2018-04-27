// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/base/lazy_now.h"

#include "base/time/tick_clock.h"

namespace blink {
namespace scheduler {

LazyNow::LazyNow(base::TimeTicks now) : tick_clock_(nullptr), now_(now) {
  DCHECK(!now.is_null());
}

LazyNow::LazyNow(const base::TickClock* tick_clock)
    : tick_clock_(tick_clock), now_() {
  DCHECK(tick_clock);
}

LazyNow::LazyNow(LazyNow&& move_from)
    : tick_clock_(move_from.tick_clock_), now_(move_from.now_) {
  move_from.tick_clock_ = nullptr;
  move_from.now_ = base::TimeTicks();
}

base::TimeTicks LazyNow::Now() {
  // TickClock might return null values only in tests, we're okay with
  // extra calls which might occur in that case.
  if (now_.is_null()) {
    DCHECK(tick_clock_);  // It can fire only on use after std::move.
    now_ = tick_clock_->NowTicks();
  }
  return now_;
}

}  // namespace scheduler
}  // namespace blink
