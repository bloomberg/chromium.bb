// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/ScopedWindowFocusAllowedIndicator.h"

#include "core/dom/ExecutionContext.h"

namespace blink {

ScopedWindowFocusAllowedIndicator::ScopedWindowFocusAllowedIndicator(
    ExecutionContext* context)
    : ContextLifecycleObserver(context)
{
    if (executionContext())
        executionContext()->allowWindowFocus();
}

ScopedWindowFocusAllowedIndicator::~ScopedWindowFocusAllowedIndicator()
{
    if (executionContext())
        executionContext()->consumeWindowFocus();
}

} // namespace blink
