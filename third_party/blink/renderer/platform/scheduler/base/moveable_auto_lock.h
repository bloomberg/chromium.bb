// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_MOVEABLE_AUTO_LOCK_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_MOVEABLE_AUTO_LOCK_H_

#include "base/synchronization/lock.h"

namespace blink {
namespace scheduler {

class MoveableAutoLock {
 public:
  explicit MoveableAutoLock(base::Lock& lock) : lock_(lock), moved_(false) {
    lock_.Acquire();
  }

  MoveableAutoLock(MoveableAutoLock&& other)
      : lock_(other.lock_), moved_(other.moved_) {
    lock_.AssertAcquired();
    other.moved_ = true;
  }

  ~MoveableAutoLock() {
    if (moved_)
      return;
    lock_.AssertAcquired();
    lock_.Release();
  }

 private:
  base::Lock& lock_;
  bool moved_;
  DISALLOW_COPY_AND_ASSIGN(MoveableAutoLock);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_MOVEABLE_AUTO_LOCK_H_
