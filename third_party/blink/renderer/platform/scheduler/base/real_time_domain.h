// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_REAL_TIME_DOMAIN_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_REAL_TIME_DOMAIN_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/base/time_domain_forward.h"

namespace base {
namespace sequence_manager {
namespace internal {

class PLATFORM_EXPORT RealTimeDomain : public TimeDomain {
 public:
  RealTimeDomain();
  ~RealTimeDomain() override;

  // TimeDomain implementation:
  LazyNow CreateLazyNow() const override;
  TimeTicks Now() const override;
  Optional<TimeDelta> DelayTillNextTask(LazyNow* lazy_now) override;

 protected:
  const char* GetName() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(RealTimeDomain);
};

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_REAL_TIME_DOMAIN_H_
