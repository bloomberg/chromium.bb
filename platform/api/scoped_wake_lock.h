// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_SCOPED_WAKE_LOCK_H_
#define PLATFORM_API_SCOPED_WAKE_LOCK_H_

#include <atomic>
#include <memory>

namespace openscreen {
namespace platform {

// Ensures that the device does not got to sleep. The wake lock
// is automatically taken as part of construction, and released as part
// of destruction.
class ScopedWakeLock {
 protected:
  // Pure virtual destructors must still be defined, but mark this class as
  // abstract.
  virtual ~ScopedWakeLock() = 0;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_SCOPED_WAKE_LOCK_H_
