/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2009, 2011, 2012 Apple Inc. All rights reserved.
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

#ifndef Notification_h
#define Notification_h

#include "heap/Handle.h"
#include "modules/notifications/NotificationBase.h"
#include "platform/AsyncMethodRunner.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class Dictionary;
class ExecutionContext;
class NotificationClient;
class NotificationPermissionCallback;

class Notification FINAL : public RefCountedWillBeRefCountedGarbageCollected<Notification>, public NotificationBase {
    DEFINE_EVENT_TARGET_REFCOUNTING(RefCountedWillBeRefCountedGarbageCollected<Notification>);

public:
    static PassRefPtrWillBeRawPtr<Notification> create(ExecutionContext*, const String& title, const Dictionary& options);

    virtual ~Notification();

    static const String& permission(ExecutionContext*);
    static void requestPermission(ExecutionContext*, PassOwnPtr<NotificationPermissionCallback> = nullptr);

    // EventTarget interface
    virtual const AtomicString& interfaceName() const OVERRIDE;

    // ActiveDOMObject interface
    virtual void stop() OVERRIDE;
    virtual bool hasPendingActivity() const OVERRIDE;

    void trace(Visitor*) { }

private:
    Notification(ExecutionContext*, const String& title, NotificationClient*);

    void showSoon();

    OwnPtr<AsyncMethodRunner<Notification> > m_asyncRunner;
};

} // namespace WebCore

#endif // Notifications_h
