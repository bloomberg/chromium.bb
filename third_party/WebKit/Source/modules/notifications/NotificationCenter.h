/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#ifndef NotificationCenter_h
#define NotificationCenter_h

#if ENABLE(LEGACY_NOTIFICATIONS)

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/html/VoidCallback.h"
#include "heap/Handle.h"
#include "modules/notifications/WebKitNotification.h"
#include "platform/Supplementable.h"
#include "platform/Timer.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class NotificationClient;
class VoidCallback;

// FIXME: oilpan: Change this to GarbageCollectedFinalized once we move ActiveDOMObject to oilpan.
class NotificationCenter FINAL : public RefCountedWillBeGarbageCollectedFinalized<NotificationCenter>, public ScriptWrappable, public ActiveDOMObject {
    DECLARE_GC_INFO;
public:
    static PassRefPtrWillBeRawPtr<NotificationCenter> create(ExecutionContext*, NotificationClient*);

    PassRefPtrWillBeRawPtr<WebKitNotification> createNotification(const String& iconUrl, const String& title, const String& body, ExceptionState& exceptionState)
    {
        if (!client()) {
            exceptionState.throwDOMException(InvalidStateError, "The notification client is null.");
            return nullptr;
        }
        return WebKitNotification::create(title, body, iconUrl, executionContext(), exceptionState, this);
    }

    NotificationClient* client() const { return m_client; }

    int checkPermission();
    void requestPermission(PassOwnPtr<VoidCallback> = nullptr);

    virtual void stop() OVERRIDE;

    void trace(Visitor*);

private:
    NotificationCenter(ExecutionContext*, NotificationClient*);

    class NotificationRequestCallback : public RefCountedWillBeGarbageCollectedFinalized<NotificationRequestCallback> {
        DECLARE_GC_INFO;
    public:
        static PassRefPtrWillBeRawPtr<NotificationRequestCallback> createAndStartTimer(NotificationCenter*, PassOwnPtr<VoidCallback>);
        void startTimer();
        void timerFired(Timer<NotificationRequestCallback>*);

        void trace(Visitor*);

    private:
        NotificationRequestCallback(NotificationCenter*, PassOwnPtr<VoidCallback>);

        RefPtrWillBeMember<NotificationCenter> m_notificationCenter;
        Timer<NotificationRequestCallback> m_timer;
        OwnPtr<VoidCallback> m_callback;
    };

    void requestTimedOut(NotificationRequestCallback*);

    NotificationClient* m_client;
    WillBeHeapHashSet<RefPtrWillBeMember<NotificationRequestCallback> > m_callbacks;
};

// FIXME: oilpan: Move this to ThreadState.h.
template<> struct ThreadingTrait<NotificationCenter::NotificationRequestCallback> {
    static const ThreadAffinity Affinity = AnyThread;
};

} // namespace WebCore

#endif // ENABLE(LEGACY_NOTIFICATIONS)

#endif // NotificationCenter_h
