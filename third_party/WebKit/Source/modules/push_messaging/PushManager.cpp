// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/push_messaging/PushManager.h"

#include "bindings/v8/CallbackPromiseAdapter.h"
#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "modules/push_messaging/PushError.h"
#include "modules/push_messaging/PushRegistration.h"
#include "public/platform/WebPushError.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

PushManager::PushManager()
{
    ScriptWrappable::init(this);
}

PushManager::~PushManager()
{
}

ScriptPromise PushManager::registerPushMessaging(ExecutionContext* context, const String& senderId)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(context);
    ScriptPromise promise = resolver->promise();
    // FIXME: Implement registration.
    OwnPtr<CallbackPromiseAdapter<PushRegistration, PushError> > adapter = adoptPtr(new CallbackPromiseAdapter<PushRegistration, PushError>(resolver, context));
    adapter->onError(new blink::WebPushError(blink::WebPushError::ErrorTypeAbort, "FIXME"));
    return promise;
}

} // namespace WebCore
