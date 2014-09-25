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

namespace blink {

namespace {

class WebNotificationPermissionCallbackImpl : public WebNotificationPermissionCallback {
public:
    WebNotificationPermissionCallbackImpl(NotificationPermissionCallback* callback)
        : m_callback(callback)
    {
    }

    virtual ~WebNotificationPermissionCallbackImpl() { }

    virtual void permissionRequestComplete(WebNotificationPermission permission) OVERRIDE
    {
        if (m_callback)
            m_callback->handleEvent(Notification::permissionString(static_cast<NotificationClient::Permission>(permission)));
    }

private:
    Persistent<NotificationPermissionCallback> m_callback;
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

void NotificationPermissionClientImpl::requestPermission(ExecutionContext* context, NotificationPermissionCallback* callback)
{
    ASSERT(context && context->isDocument());

    Document* document = toDocument(context);
    WebLocalFrameImpl* webFrame = WebLocalFrameImpl::fromFrame(document->frame());

    webFrame->client()->requestNotificationPermission(WebSecurityOrigin(context->securityOrigin()), new WebNotificationPermissionCallbackImpl(callback));
}

} // namespace blink
