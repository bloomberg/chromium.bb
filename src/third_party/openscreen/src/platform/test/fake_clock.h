// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_TEST_FAKE_CLOCK_H_
#define PLATFORM_TEST_FAKE_CLOCK_H_

#include <vector>

#include "platform/api/time.h"

namespace openscreen {
namespace platform {

class FakeTaskRunner;

class FakeClock {
 public:
  explicit FakeClock(Clock::time_point start_time);
  ~FakeClock();

  void Advance(Clock::duration delta);

  static Clock::time_point now() noexcept;

 protected:
  friend class FakeTaskRunner;

  void SubscribeToTimeChanges(FakeTaskRunner* task_runner);
  void UnsubscribeFromTimeChanges(FakeTaskRunner* task_runner);

 private:
  std::vector<FakeTaskRunner*> task_runners_;

  static FakeClock* instance_;
  static Clock::time_point now_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_TEST_FAKE_CLOCK_H_
