// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_FAKE_COMPOSITOR_LOCK_H_
#define UI_COMPOSITOR_FAKE_COMPOSITOR_LOCK_H_

#include "ui/compositor/compositor_lock.h"

namespace ui {

class FakeCompositorLock : public CompositorLock {
 public:
  explicit FakeCompositorLock(CompositorLockClient* client,
                              base::WeakPtr<CompositorLockDelegate> delegate);

  // Allow tests to cause the lock to timeout.
  using CompositorLock::TimeoutLock;
};

}  // namespace

#endif  // UI_COMPOSITOR_FAKE_COMPOSITOR_LOCK_H_
