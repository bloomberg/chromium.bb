/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CustomElementCallbackDispatcher_h
#define CustomElementCallbackDispatcher_h

#include "core/dom/CustomElementLifecycleCallbacks.h"
#include "core/dom/Element.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class CustomElementCallbackDispatcher {
    WTF_MAKE_NONCOPYABLE(CustomElementCallbackDispatcher);
public:
    static CustomElementCallbackDispatcher& instance();

    class CallbackDeliveryScope {
    public:
        CallbackDeliveryScope() { }
        ~CallbackDeliveryScope()
        {
            CustomElementCallbackDispatcher& dispatcher = CustomElementCallbackDispatcher::instance();
            if (dispatcher.hasQueuedCallbacks())
                dispatcher.dispatch();
        }
    };

    void enqueueReadyCallback(CustomElementLifecycleCallbacks*, Element*);

    // Returns true if more work may have to be performed at the
    // checkpoint by this or other workers (for example, this work
    // invoked author scripts)
    bool dispatch();

private:
    explicit CustomElementCallbackDispatcher() { }

    bool hasQueuedCallbacks() { return !m_invocations.isEmpty(); }

    class ReadyInvocation {
    public:
        ReadyInvocation(PassRefPtr<CustomElementLifecycleCallbacks>, PassRefPtr<Element>);
        virtual ~ReadyInvocation() { }
        void invoke() { m_callbacks->ready(m_element.get()); }

    private:
        RefPtr<CustomElementLifecycleCallbacks> m_callbacks;
        RefPtr<Element> m_element;
    };

    Vector<ReadyInvocation> m_invocations;
};

}

#endif // CustomElementCallbackDispatcher_h
