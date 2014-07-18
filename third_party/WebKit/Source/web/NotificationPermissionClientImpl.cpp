// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/NotificationPermissionClientImpl.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "modules/notifications/Notification.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebNotificationPermissionCallback.h"
#include "web/WebLocalFrameImpl.h"

using namespace blink;

namespace blink {

namespace {

class WebNotificationPermissionCallbackImpl : public WebNotificationPermissionCallback {
public:
    WebNotificationPermissionCallbackImpl(ExecutionContext* executionContext, PassOwnPtr<NotificationPermissionCallback> callback)
        : m_executionContext(executionContext)
        , m_callback(callback)
    {
    }

    virtual ~WebNotificationPermissionCallbackImpl() { }

    virtual void permissionRequestComplete(WebNotificationPermission permission) OVERRIDE
    {
        m_callback->handleEvent(Notification::permissionString(static_cast<NotificationClient::Permission>(permission)));
    }

    // FIXME: Deprecated. Notification::permission() requires another round-trip
    // to the browser process using a synchronous IPC.
    virtual void permissionRequestComplete() OVERRIDE
    {
        m_callback->handleEvent(Notification::permission(m_executionContext));
    }

private:
    ExecutionContext* m_executionContext;
    OwnPtr<NotificationPermissionCallback> m_callback;
};

} // namespace

PassOwnPtrWillBeRawPtr<NotificationPermissionClientImpl> NotificationPermissionClientImpl::create()
{
    return adoptPtrWillBeNoop(new NotificationPermissionClientImpl());
}

NotificationPermissionClientImpl::NotificationPermissionClientImpl()
{
}

NotificationPermissionClientImpl::~NotificationPermissionClientImpl()
{
}

void NotificationPermissionClientImpl::requestPermission(ExecutionContext* context, PassOwnPtr<NotificationPermissionCallback> callback)
{
    ASSERT(context && context->isDocument());
    ASSERT(callback);

    Document* document = toDocument(context);
    WebLocalFrameImpl* webFrame = WebLocalFrameImpl::fromFrame(document->frame());

    webFrame->client()->requestNotificationPermission(WebSecurityOrigin(context->securityOrigin()), new WebNotificationPermissionCallbackImpl(context, callback));
}

} // namespace blink
