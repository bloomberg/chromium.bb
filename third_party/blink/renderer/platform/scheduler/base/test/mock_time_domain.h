// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_TEST_MOCK_TIME_DOMAIN_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_TEST_MOCK_TIME_DOMAIN_H_

#include "third_party/blink/renderer/platform/scheduler/base/time_domain_forward.h"

namespace base {
namespace sequence_manager {

// TimeDomain with a mock clock and not invoking SequenceManager.
// NOTE: All methods are main thread only.
class MockTimeDomain : public TimeDomain {
 public:
  explicit MockTimeDomain(TimeTicks initial_now_ticks);
  ~MockTimeDomain() override;

  void SetNowTicks(TimeTicks now_ticks);

  // TimeDomain implementation:
  LazyNow CreateLazyNow() const override;
  TimeTicks Now() const override;
  Optional<TimeDelta> DelayTillNextTask(LazyNow* lazy_now) override;
  void SetNextDelayedDoWork(LazyNow* lazy_now, TimeTicks run_time) override;
  const char* GetName() const override;

 private:
  TimeTicks now_ticks_;

  DISALLOW_COPY_AND_ASSIGN(MockTimeDomain);
};

}  // namespace sequence_manager
}  // namespace base

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_TEST_MOCK_TIME_DOMAIN_H_
