// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/PushPermissionClientImpl.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "public/web/WebFrameClient.h"
#include "web/WebLocalFrameImpl.h"

namespace blink {

PassOwnPtrWillBeRawPtr<PushPermissionClientImpl> PushPermissionClientImpl::create()
{
    return adoptPtrWillBeNoop(new PushPermissionClientImpl());
}

PushPermissionClientImpl::PushPermissionClientImpl()
{
}

PushPermissionClientImpl::~PushPermissionClientImpl()
{
}

void PushPermissionClientImpl::requestPermission(ExecutionContext* context, WebCallback* callback)
{
    ASSERT(context && context->isDocument());

    Document* document = toDocument(context);
    WebLocalFrameImpl* webFrame = WebLocalFrameImpl::fromFrame(document->frame());

    webFrame->client()->requestPushPermission(callback);
}

} // namespace blink
