// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/draw_waiter_for_test.h"

#include "ui/compositor/compositor.h"

namespace ui {

// static
void DrawWaiterForTest::Wait(Compositor* compositor) {
  DrawWaiterForTest waiter;
  waiter.wait_for_commit_ = false;
  waiter.WaitImpl(compositor);
}

// static
void DrawWaiterForTest::WaitForCommit(Compositor* compositor) {
  DrawWaiterForTest waiter;
  waiter.wait_for_commit_ = true;
  waiter.WaitImpl(compositor);
}

DrawWaiterForTest::DrawWaiterForTest() {}

DrawWaiterForTest::~DrawWaiterForTest() {}

void DrawWaiterForTest::WaitImpl(Compositor* compositor) {
  compositor->AddObserver(this);
  wait_run_loop_.reset(new base::RunLoop());
  wait_run_loop_->Run();
  compositor->RemoveObserver(this);
}

void DrawWaiterForTest::OnCompositingDidCommit(Compositor* compositor) {
  if (wait_for_commit_)
    wait_run_loop_->Quit();
}

void DrawWaiterForTest::OnCompositingStarted(Compositor* compositor,
                                             base::TimeTicks start_time) {}

void DrawWaiterForTest::OnCompositingEnded(Compositor* compositor) {
  if (!wait_for_commit_)
    wait_run_loop_->Quit();
}

void DrawWaiterForTest::OnCompositingAborted(Compositor* compositor) {}

void DrawWaiterForTest::OnCompositingLockStateChanged(Compositor* compositor) {}

}  // namespace ui
