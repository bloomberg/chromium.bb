// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollStateCallback_h
#define ScrollStateCallback_h

#include "platform/heap/Handle.h"

namespace blink {

class ScrollState;

enum class NativeScrollBehavior {
    DisableNativeScroll,
    PerformBeforeNativeScroll,
    PerformAfterNativeScroll,
};

class ScrollStateCallback : public GarbageCollectedFinalized<ScrollStateCallback> {
public:
    ScrollStateCallback()
        : m_nativeScrollBehavior(NativeScrollBehavior::DisableNativeScroll)
    {
    }

    virtual ~ScrollStateCallback()
    {
    }

    DEFINE_INLINE_VIRTUAL_TRACE() {}
    virtual void handleEvent(ScrollState*) = 0;

    void setNativeScrollBehavior(NativeScrollBehavior nativeScrollBehavior)
    {
        m_nativeScrollBehavior = nativeScrollBehavior;
    }

    NativeScrollBehavior nativeScrollBehavior()
    {
        return m_nativeScrollBehavior;
    }

    static NativeScrollBehavior toNativeScrollBehavior(String nativeScrollBehavior);

protected:
    NativeScrollBehavior m_nativeScrollBehavior;
};

} // namespace blink

#endif // ScrollStateCallback_h
