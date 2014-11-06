// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/push_messaging/PushPermissionClient.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"

namespace blink {

const char* PushPermissionClient::supplementName()
{
    return "PushPermissionClient";
}

PushPermissionClient* PushPermissionClient::from(ExecutionContext* context)
{
    if (!context->isDocument())
        return nullptr;

    const Document* document = toDocument(context);
    ASSERT(document->frame());

    if (!document->frame()->isLocalFrame())
        return nullptr;

    return static_cast<PushPermissionClient*>(WillBeHeapSupplement<LocalFrame>::from(document->frame(), supplementName()));
}

void providePushPermissionClientTo(LocalFrame& frame, PassOwnPtrWillBeRawPtr<PushPermissionClient> client)
{
    frame.provideSupplement(PushPermissionClient::supplementName(), client);
}

}
