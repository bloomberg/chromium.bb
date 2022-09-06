// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "internal/platform/implementation/ios/condition_variable.h"

#include "internal/platform/implementation/ios/mutex.h"

namespace location {
namespace nearby {
namespace ios {

Exception ConditionVariable::Wait() {
  condition_variable_.Wait(mutex_);
  return {Exception::kSuccess};
}

Exception ConditionVariable::Wait(absl::Duration timeout) {
  condition_variable_.WaitWithTimeout(mutex_, timeout);
  return {Exception::kSuccess};
}

void ConditionVariable::Notify() { condition_variable_.SignalAll(); }

}  // namespace ios
}  // namespace nearby
}  // namespace location
