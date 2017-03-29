// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor_lock.h"

namespace ui {

CompositorLock::CompositorLock(CompositorLockClient* client,
                               base::WeakPtr<CompositorLockDelegate> delegate)
    : client_(client), delegate_(std::move(delegate)) {}

CompositorLock::~CompositorLock() {
  if (delegate_)
    delegate_->RemoveCompositorLock(this);
}

void CompositorLock::TimeoutLock() {
  delegate_->RemoveCompositorLock(this);
  delegate_ = nullptr;
  if (client_)
    client_->CompositorLockTimedOut();
}

}  // namespace ui
