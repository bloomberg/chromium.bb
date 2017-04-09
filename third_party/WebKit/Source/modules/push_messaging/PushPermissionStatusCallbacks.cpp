// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/push_messaging/PushPermissionStatusCallbacks.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/push_messaging/PushError.h"
#include "wtf/Assertions.h"
#include "wtf/text/WTFString.h"

namespace blink {

PushPermissionStatusCallbacks::PushPermissionStatusCallbacks(
    ScriptPromiseResolver* resolver)
    : resolver_(resolver) {}

PushPermissionStatusCallbacks::~PushPermissionStatusCallbacks() {}

void PushPermissionStatusCallbacks::OnSuccess(WebPushPermissionStatus status) {
  resolver_->Resolve(PermissionString(status));
}

void PushPermissionStatusCallbacks::OnError(const WebPushError& error) {
  if (!resolver_->GetExecutionContext() ||
      resolver_->GetExecutionContext()->IsContextDestroyed())
    return;
  resolver_->Reject(PushError::Take(resolver_.Get(), error));
}

// static
String PushPermissionStatusCallbacks::PermissionString(
    WebPushPermissionStatus status) {
  switch (status) {
    case kWebPushPermissionStatusGranted:
      return "granted";
    case kWebPushPermissionStatusDenied:
      return "denied";
    case kWebPushPermissionStatusPrompt:
      return "prompt";
  }

  NOTREACHED();
  return "denied";
}

}  // namespace blink
