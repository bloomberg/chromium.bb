// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_display_lock_callback.h"

namespace blink {

DisplayLockContext::~DisplayLockContext() {}

void DisplayLockContext::Dispose() {}

void DisplayLockContext::schedule(V8DisplayLockCallback* callback) {
  callback->InvokeAndReportException(nullptr, this);
}

}  // namespace blink
