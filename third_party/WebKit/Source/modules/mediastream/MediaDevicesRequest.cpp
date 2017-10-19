/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/mediastream/MediaDevicesRequest.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "modules/mediastream/UserMediaController.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

MediaDevicesRequest* MediaDevicesRequest::Create(
    ScriptState* state,
    UserMediaController* controller) {
  return new MediaDevicesRequest(state, controller);
}

MediaDevicesRequest::MediaDevicesRequest(ScriptState* state,
                                         UserMediaController* controller)
    : ContextLifecycleObserver(ExecutionContext::From(state)),
      controller_(controller),
      resolver_(ScriptPromiseResolver::Create(state)) {}

MediaDevicesRequest::~MediaDevicesRequest() {}

Document* MediaDevicesRequest::OwnerDocument() {
  if (ExecutionContext* context = GetExecutionContext()) {
    return ToDocument(context);
  }

  return nullptr;
}

ScriptPromise MediaDevicesRequest::Start() {
  DCHECK(controller_);
  resolver_->KeepAliveWhilePending();
  controller_->RequestMediaDevices(this);
  return resolver_->Promise();
}

void MediaDevicesRequest::Succeed(const MediaDeviceInfoVector& media_devices) {
  if (!GetExecutionContext() || !resolver_)
    return;

  resolver_->Resolve(media_devices);
}

void MediaDevicesRequest::ContextDestroyed(ExecutionContext*) {
  controller_.Clear();
  resolver_.Clear();
}

void MediaDevicesRequest::Trace(blink::Visitor* visitor) {
  visitor->Trace(controller_);
  visitor->Trace(resolver_);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
