// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/peerconnection/RTCSessionDescriptionRequestPromiseImpl.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/peerconnection/RTCPeerConnection.h"
#include "modules/peerconnection/RTCSessionDescription.h"
#include "public/platform/WebRTCSessionDescription.h"

namespace blink {

RTCSessionDescriptionRequestPromiseImpl*
RTCSessionDescriptionRequestPromiseImpl::Create(
    RTCPeerConnection* requester,
    ScriptPromiseResolver* resolver) {
  return new RTCSessionDescriptionRequestPromiseImpl(requester, resolver);
}

RTCSessionDescriptionRequestPromiseImpl::
    RTCSessionDescriptionRequestPromiseImpl(RTCPeerConnection* requester,
                                            ScriptPromiseResolver* resolver)
    : requester_(requester), resolver_(resolver) {
  DCHECK(requester_);
  DCHECK(resolver_);
}

RTCSessionDescriptionRequestPromiseImpl::
    ~RTCSessionDescriptionRequestPromiseImpl() {}

void RTCSessionDescriptionRequestPromiseImpl::RequestSucceeded(
    const WebRTCSessionDescription& web_session_description) {
  if (requester_ && requester_->ShouldFireDefaultCallbacks()) {
    resolver_->Resolve(RTCSessionDescription::Create(web_session_description));
  } else {
    // This is needed to have the resolver release its internal resources
    // while leaving the associated promise pending as specified.
    resolver_->Detach();
  }

  Clear();
}

void RTCSessionDescriptionRequestPromiseImpl::RequestFailed(
    const String& error) {
  if (requester_ && requester_->ShouldFireDefaultCallbacks()) {
    // TODO(guidou): The error code should come from the content layer. See
    // crbug.com/589455
    resolver_->Reject(DOMException::Create(kOperationError, error));
  } else {
    // This is needed to have the resolver release its internal resources
    // while leaving the associated promise pending as specified.
    resolver_->Detach();
  }

  Clear();
}

void RTCSessionDescriptionRequestPromiseImpl::Clear() {
  requester_.Clear();
}

void RTCSessionDescriptionRequestPromiseImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(resolver_);
  visitor->Trace(requester_);
  RTCSessionDescriptionRequest::Trace(visitor);
}

}  // namespace blink
