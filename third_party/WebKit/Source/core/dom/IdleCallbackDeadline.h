// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IdleCallbackDeadline_h
#define IdleCallbackDeadline_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class IdleCallbackDeadline : public GarbageCollected<IdleCallbackDeadline>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    enum class CallbackType {
        CalledWhenIdle,
        CalledByTimeout
    };

    static IdleCallbackDeadline* create(double deadlineSeconds, CallbackType callbackType)
    {
        return new IdleCallbackDeadline(deadlineSeconds, callbackType);
    }

    DEFINE_INLINE_TRACE() { }

    double timeRemaining() const;

    bool didTimeout() const
    {
        return m_callbackType == CallbackType::CalledByTimeout;
    }

private:
    IdleCallbackDeadline(double deadlineSeconds, CallbackType);

    double m_deadlineSeconds;
    CallbackType m_callbackType;
};

} // namespace blink

#endif // IdleCallbackDeadline_h
