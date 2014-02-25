/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef WebKitNotification_h
#define WebKitNotification_h

#if ENABLE(LEGACY_NOTIFICATIONS)

#include "core/events/EventTarget.h"
#include "heap/Handle.h"
#include "modules/notifications/NotificationBase.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class ExceptionState;
class ExecutionContext;
class NotificationCenter;

// Implementation of the legacy notification API as specified the following page:
// http://chromium.org/developers/design-documents/desktop-notifications/api-specification
class WebKitNotification FINAL : public RefCountedWillBeRefCountedGarbageCollected<WebKitNotification>, public NotificationBase {
    DEFINE_EVENT_TARGET_REFCOUNTING(RefCountedWillBeRefCountedGarbageCollected<WebKitNotification>);

public:
    static PassRefPtrWillBeRawPtr<WebKitNotification> create(const String& title, const String& body, const String& iconUrl, ExecutionContext*, ExceptionState&, PassRefPtrWillBeRawPtr<NotificationCenter> provider);

    virtual ~WebKitNotification();

    void cancel() { close(); }

    DEFINE_MAPPED_ATTRIBUTE_EVENT_LISTENER(display, show);

    String replaceId() const { return tag(); }
    void setReplaceId(const String& replaceId) { setTag(replaceId); }

    // EventTarget interface
    virtual const AtomicString& interfaceName() const OVERRIDE;

    void trace(Visitor*) { }

private:
    WebKitNotification(const String& title, const String& body, const String& iconUrl, ExecutionContext*, ExceptionState&, PassRefPtrWillBeRawPtr<NotificationCenter> provider);
};

} // namespace WebCore

#endif // ENABLE(LEGACY_NOTIFICATIONS)

#endif // WebKitNotification_h
