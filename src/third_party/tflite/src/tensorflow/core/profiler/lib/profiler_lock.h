/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#ifndef TENSORFLOW_CORE_PROFILER_LIB_PROFILER_LOCK_H_
#define TENSORFLOW_CORE_PROFILER_LIB_PROFILER_LOCK_H_

#include "tensorflow/core/platform/statusor.h"

namespace tensorflow {
namespace profiler {

// Handle for the profiler lock. At most one instance of this class, the
// "active" instance, owns the profiler lock.
class ProfilerLock {
 public:
  // Acquires the profiler lock if no other profiler session is currently
  // active.
  static StatusOr<ProfilerLock> Acquire();

  // Default constructor creates an inactive instance.
  ProfilerLock() = default;

  // Non-copyable.
  ProfilerLock(const ProfilerLock&) = delete;
  ProfilerLock& operator=(const ProfilerLock&) = delete;

  // Movable.
  ProfilerLock(ProfilerLock&& other)
      : active_(std::exchange(other.active_, false)) {}
  ProfilerLock& operator=(ProfilerLock&& other) {
    active_ = std::exchange(other.active_, false);
    return *this;
  }

  ~ProfilerLock() { ReleaseIfActive(); }

  // Allow creating another active instance.
  void ReleaseIfActive();

  // Returs true if this is the active instance.
  bool Active() const { return active_; }

 private:
  // Explicit constructor allows creating an active instance, private so it can
  // only be called by Acquire.
  explicit ProfilerLock(bool active) : active_(active) {}

  bool active_ = false;
};

}  // namespace profiler
}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_PROFILER_LIB_PROFILER_LOCK_H_
