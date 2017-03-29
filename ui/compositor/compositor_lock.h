// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_LOCK_H_
#define UI_COMPOSITOR_COMPOSITOR_LOCK_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/compositor/compositor_export.h"

namespace ui {

class Compositor;
class CompositorLock;

class CompositorLockClient {
 public:
  virtual ~CompositorLockClient() {}

  // Called if the CompositorLock ends before being destroyed due to timeout.
  virtual void CompositorLockTimedOut() = 0;
};

class CompositorLockDelegate {
 public:
  virtual ~CompositorLockDelegate() {}

  // Called to perform the unlock operation.
  virtual void RemoveCompositorLock(CompositorLock*) = 0;
};

// This class represents a lock on the compositor, that can be used to prevent
// commits to the compositor tree while we're waiting for an asynchronous
// event. The typical use case is when waiting for a renderer to produce a frame
// at the right size. The caller keeps a reference on this object, and drops the
// reference once it desires to release the lock.
// By default, the lock will be cancelled after a short timeout to ensure
// responsiveness of the UI, so the compositor tree should be kept in a
// "reasonable" state while the lock is held. The timeout length, or no timeout,
// can be requested at the time the lock is created.
// Don't instantiate this class directly, use Compositor::GetCompositorLock.
class COMPOSITOR_EXPORT CompositorLock {
 public:
  // The |client| is informed about events from the CompositorLock. The
  // |delegate| is used to perform actual unlocking. If |timeout| is zero then
  // no timeout is scheduled, else a timeout is scheduled on the |task_runner|.
  explicit CompositorLock(CompositorLockClient* client,
                          base::WeakPtr<CompositorLockDelegate> delegate);
  ~CompositorLock();

 private:
  friend class Compositor;
  friend class FakeCompositorLock;

  // Causes the CompositorLock to end due to a timeout.
  void TimeoutLock();

  CompositorLockClient* const client_;
  base::WeakPtr<CompositorLockDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(CompositorLock);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_COMPOSITOR_LOCK_H_
