// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ContextLifecycleObserver.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"

namespace blink {

LocalFrame* ContextLifecycleObserver::frame() const {
  return getExecutionContext() && getExecutionContext()->isDocument()
             ? toDocument(getExecutionContext())->frame()
             : nullptr;
}
}
