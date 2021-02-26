// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_CONDITION_VARIABLE_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_CONDITION_VARIABLE_H_

#include "base/synchronization/condition_variable.h"
#include "third_party/abseil-cpp/absl/time/time.h"
#include "third_party/nearby/src/cpp/platform/api/condition_variable.h"

namespace location {
namespace nearby {
namespace chrome {

class Mutex;

// Concrete ConditionVariable implementation.
class ConditionVariable : public api::ConditionVariable {
 public:
  explicit ConditionVariable(Mutex* mutex);
  ~ConditionVariable() override;

  ConditionVariable(const ConditionVariable&) = delete;
  ConditionVariable& operator=(const ConditionVariable&) = delete;

  // api::ConditionVariable:
  Exception Wait() override;
  Exception Wait(absl::Duration timeout) override;
  void Notify() override;

 private:
  Mutex* mutex_;
  base::ConditionVariable condition_variable_;
};

}  // namespace chrome
}  // namespace nearby
}  // namespace location

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_CONDITION_VARIABLE_H_
