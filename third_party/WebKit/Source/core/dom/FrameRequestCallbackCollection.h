// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FrameRequestCallbackCollection_h
#define FrameRequestCallbackCollection_h

#include "platform/heap/Handle.h"

namespace blink {

class ExecutionContext;
class FrameRequestCallback;

class FrameRequestCallbackCollection final {
public:
    explicit FrameRequestCallbackCollection(ExecutionContext*);

    typedef int CallbackId;
    CallbackId registerCallback(FrameRequestCallback*);
    void cancelCallback(CallbackId);
    void executeCallbacks(double highResNowMs, double highResNowMsLegacy);

    bool isEmpty() const { return !m_callbacks.size(); }

    DECLARE_TRACE();

private:
    typedef PersistentHeapVectorWillBeHeapVector<Member<FrameRequestCallback>> CallbackList;
    CallbackList m_callbacks;
    CallbackList m_callbacksToInvoke; // only non-empty while inside executeCallbacks

    CallbackId m_nextCallbackId = 0;

    ExecutionContext* m_context = nullptr;
};

} // namespace blink

#endif // FrameRequestCallbackCollection_h
