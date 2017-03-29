// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/fake_compositor_lock.h"

namespace ui {

FakeCompositorLock::FakeCompositorLock(
    CompositorLockClient* client,
    base::WeakPtr<CompositorLockDelegate> delegate)
    : CompositorLock(client, delegate) {}

}  // namespace
