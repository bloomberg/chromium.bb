// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScopedWindowFocusAllowedIndicator_h
#define ScopedWindowFocusAllowedIndicator_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/ExecutionContext.h"
#include "wtf/Noncopyable.h"

namespace blink {

class ScopedWindowFocusAllowedIndicator final {
    WTF_MAKE_NONCOPYABLE(ScopedWindowFocusAllowedIndicator);
public:
    explicit ScopedWindowFocusAllowedIndicator(ExecutionContext* executionContext)
        : m_observer(adoptPtrWillBeNoop(new Observer(executionContext)))
    {
    }
    ~ScopedWindowFocusAllowedIndicator()
    {
        m_observer->dispose();
    }

private:
    class Observer final : public NoBaseWillBeGarbageCollectedFinalized<Observer>, public ContextLifecycleObserver {
    public:
        explicit Observer(ExecutionContext* executionContext)
            : ContextLifecycleObserver(executionContext)
        {
            if (executionContext)
                executionContext->allowWindowFocus();
        }

        void dispose()
        {
            if (executionContext())
                executionContext()->consumeWindowFocus();
        }
    };

    // In Oilpan, destructors are not allowed to touch other on-heap objects.
    // The Observer indirection is needed to keep
    // ScopedWindowFocusAllowedIndicator off-heap and thus allows its destructor
    // to call executionContext()->consumeWindowFocus().
    OwnPtrWillBePersistent<Observer> m_observer;
};

} // namespace blink

#endif // ScopedWindowFocusAllowedIndicator_h
