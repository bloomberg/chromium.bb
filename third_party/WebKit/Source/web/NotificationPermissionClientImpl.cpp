// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/NotificationPermissionClientImpl.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "modules/notifications/Notification.h"
#include "modules/notifications/NotificationPermissionCallback.h"
#include "public/web/WebFrameClient.h"
#include "public/web/modules/notifications/WebNotificationPermissionCallback.h"
#include "web/WebLocalFrameImpl.h"

namespace blink {

namespace {

class WebNotificationPermissionCallbackImpl : public WebNotificationPermissionCallback {
public:
    WebNotificationPermissionCallbackImpl(ScriptPromiseResolver* resolver, NotificationPermissionCallback* deprecatedCallback)
        : m_resolver(resolver)
        , m_deprecatedCallback(deprecatedCallback)
    {
    }

    ~WebNotificationPermissionCallbackImpl() override { }

    void permissionRequestComplete(WebNotificationPermission permission) override
    {
        String permissionString = Notification::permissionString(permission);
        if (m_deprecatedCallback)
            m_deprecatedCallback->handleEvent(permissionString);

        m_resolver->resolve(permissionString);
    }

private:
    Persistent<ScriptPromiseResolver> m_resolver;
    Persistent<NotificationPermissionCallback> m_deprecatedCallback;
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

ScriptPromise NotificationPermissionClientImpl::requestPermission(ScriptState* scriptState, NotificationPermissionCallback* deprecatedCallback)
{
    ASSERT(scriptState);

    ExecutionContext* context = scriptState->executionContext();
    ASSERT(context && context->isDocument());

    Document* document = toDocument(context);
    WebLocalFrameImpl* webFrame = WebLocalFrameImpl::fromFrame(document->frame());

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    webFrame->client()->requestNotificationPermission(WebSecurityOrigin(context->securityOrigin()), new WebNotificationPermissionCallbackImpl(resolver, deprecatedCallback));

    return promise;
}

} // namespace blink
