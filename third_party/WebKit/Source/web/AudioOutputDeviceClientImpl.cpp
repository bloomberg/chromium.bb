// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/AudioOutputDeviceClientImpl.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "public/web/WebFrameClient.h"
#include "web/WebLocalFrameImpl.h"

namespace blink {

PassOwnPtrWillBeRawPtr<AudioOutputDeviceClientImpl> AudioOutputDeviceClientImpl::create()
{
    return adoptPtrWillBeNoop(new AudioOutputDeviceClientImpl());
}

AudioOutputDeviceClientImpl::AudioOutputDeviceClientImpl()
{
}

AudioOutputDeviceClientImpl::~AudioOutputDeviceClientImpl()
{
}

void AudioOutputDeviceClientImpl::checkIfAudioSinkExistsAndIsAuthorized(ExecutionContext* context, const WebString& sinkId, PassOwnPtr<WebSetSinkIdCallbacks> callbacks)
{
    ASSERT(context && context->isDocument());
    Document* document = toDocument(context);
    WebLocalFrameImpl* webFrame = WebLocalFrameImpl::fromFrame(document->frame());
    webFrame->client()->checkIfAudioSinkExistsAndIsAuthorized(sinkId, WebSecurityOrigin(context->securityOrigin()), callbacks.leakPtr());
}

} // namespace blink
