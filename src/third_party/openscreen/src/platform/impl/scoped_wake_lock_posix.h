// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_SCOPED_WAKE_LOCK_POSIX_H_
#define PLATFORM_IMPL_SCOPED_WAKE_LOCK_POSIX_H_

#include <atomic>
#include <memory>

#include "platform/api/scoped_wake_lock.h"

namespace openscreen {
namespace platform {

class ScopedWakeLockPosix : public ScopedWakeLock {
 public:
  ScopedWakeLockPosix();

 protected:
  virtual ~ScopedWakeLockPosix() override;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_SCOPED_WAKE_LOCK_POSIX_H_
