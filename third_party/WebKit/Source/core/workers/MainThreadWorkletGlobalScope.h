// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MainThreadWorkletGlobalScope_h
#define MainThreadWorkletGlobalScope_h

#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrameLifecycleObserver.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"

namespace blink {

class LocalFrame;

class CORE_EXPORT MainThreadWorkletGlobalScope : public WorkerOrWorkletGlobalScope, public LocalFrameLifecycleObserver {
public:
    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        WorkerOrWorkletGlobalScope::trace(visitor);
        LocalFrameLifecycleObserver::trace(visitor);
    }

protected:
    explicit MainThreadWorkletGlobalScope(LocalFrame* frame)
        : LocalFrameLifecycleObserver(frame) { }
};

DEFINE_TYPE_CASTS(MainThreadWorkletGlobalScope, ExecutionContext, context, context->isWorkletGlobalScope(), context.isWorkletGlobalScope());

} // namespace blink

#endif // MainThreadWorkletGlobalScope_h
