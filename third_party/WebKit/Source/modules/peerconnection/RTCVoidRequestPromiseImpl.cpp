// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/peerconnection/RTCVoidRequestPromiseImpl.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/peerconnection/RTCPeerConnection.h"

namespace blink {

RTCVoidRequestPromiseImpl* RTCVoidRequestPromiseImpl::Create(
    RTCPeerConnection* requester,
    ScriptPromiseResolver* resolver) {
  return new RTCVoidRequestPromiseImpl(requester, resolver);
}

RTCVoidRequestPromiseImpl::RTCVoidRequestPromiseImpl(
    RTCPeerConnection* requester,
    ScriptPromiseResolver* resolver)
    : requester_(requester), resolver_(resolver) {
  DCHECK(requester_);
  DCHECK(resolver_);
}

RTCVoidRequestPromiseImpl::~RTCVoidRequestPromiseImpl() = default;

void RTCVoidRequestPromiseImpl::RequestSucceeded() {
  if (requester_ && requester_->ShouldFireDefaultCallbacks()) {
    resolver_->Resolve();
  } else {
    // This is needed to have the resolver release its internal resources
    // while leaving the associated promise pending as specified.
    resolver_->Detach();
  }

  Clear();
}

void RTCVoidRequestPromiseImpl::RequestFailed(const String& error) {
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

void RTCVoidRequestPromiseImpl::Clear() {
  requester_.Clear();
}

void RTCVoidRequestPromiseImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(resolver_);
  visitor->Trace(requester_);
  RTCVoidRequest::Trace(visitor);
}

}  // namespace blink
