// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IdleCallbackDeadline_h
#define IdleCallbackDeadline_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class DocumentLoadTiming;

class IdleCallbackDeadline : public GarbageCollected<IdleCallbackDeadline>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();

public:
    enum class CallbackType {
        CalledWhenIdle,
        CalledByTimeout
    };

    DEFINE_INLINE_TRACE() {}
    static IdleCallbackDeadline* create(double deadlineMillis, CallbackType callbackType, const DocumentLoadTiming& timing)
    {
        return new IdleCallbackDeadline(deadlineMillis, callbackType, timing);
    }

    double timeRemaining() const;

    bool didTimeout() const
    {
        return m_callbackType == CallbackType::CalledByTimeout;
    }

private:
    IdleCallbackDeadline(double deadlineMillis, CallbackType, const DocumentLoadTiming&);

    double m_deadlineMillis;
    CallbackType m_callbackType;
    const DocumentLoadTiming& m_timing;
};
}

#endif // IdleCallbackDeadline_h
