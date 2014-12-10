// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScopedWindowFocusAllowedIndicator_h
#define ScopedWindowFocusAllowedIndicator_h

#include "core/dom/ContextLifecycleObserver.h"
#include "wtf/Noncopyable.h"

namespace blink {

class ExecutionContext;

class ScopedWindowFocusAllowedIndicator : ContextLifecycleObserver {
    WTF_MAKE_NONCOPYABLE(ScopedWindowFocusAllowedIndicator);
public:
    explicit ScopedWindowFocusAllowedIndicator(ExecutionContext*);
    ~ScopedWindowFocusAllowedIndicator();
};

} // namespace blink

#endif // ScopedWindowFocusAllowedIndicator_h
