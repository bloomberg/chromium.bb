// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/push_messaging/PushManager.h"

#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "bindings/v8/ScriptValue.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"

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
    resolver->reject(ScriptValue::createNull());
    return promise;
}

} // namespace WebCore
