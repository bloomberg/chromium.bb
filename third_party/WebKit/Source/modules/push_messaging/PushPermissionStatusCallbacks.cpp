// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/push_messaging/PushPermissionStatusCallbacks.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/ExceptionCode.h"
#include "wtf/text/WTFString.h"

namespace blink {

PushPermissionStatusCallbacks::PushPermissionStatusCallbacks(PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver)
    : m_resolver(resolver)
{
}

PushPermissionStatusCallbacks::~PushPermissionStatusCallbacks()
{
}

void PushPermissionStatusCallbacks::onSuccess(WebPushPermissionStatus* status)
{
    m_resolver->resolve(permissionString(*status));
}

void PushPermissionStatusCallbacks::onError()
{
    m_resolver->reject();
}

// static
String PushPermissionStatusCallbacks::permissionString(WebPushPermissionStatus status)
{
    switch (status) {
    case WebPushPermissionStatusGranted:
        return "granted";
    case WebPushPermissionStatusDenied:
        return "denied";
    case WebPushPermissionStatusDefault:
        return "default";
    }

    ASSERT_NOT_REACHED();
    return "denied";
}

} // namespace blink
