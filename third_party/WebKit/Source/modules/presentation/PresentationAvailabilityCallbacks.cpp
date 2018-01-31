// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationAvailabilityCallbacks.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "modules/presentation/PresentationAvailability.h"
#include "modules/presentation/PresentationError.h"
#include "modules/presentation/PresentationRequest.h"

namespace blink {

namespace {

DOMException* CreateAvailabilityNotSupportedError() {
  static const WebString& not_supported_error = blink::WebString::FromUTF8(
      "getAvailability() isn't supported at the moment. It can be due to "
      "a permanent or temporary system limitation. It is recommended to "
      "try to blindly start a presentation in that case.");
  return DOMException::Create(kNotSupportedError, not_supported_error);
}

}  // namespace

PresentationAvailabilityCallbacks::PresentationAvailabilityCallbacks(
    PresentationAvailabilityProperty* resolver,
    const Vector<KURL>& urls)
    : resolver_(resolver), urls_(urls) {}

PresentationAvailabilityCallbacks::~PresentationAvailabilityCallbacks() =
    default;

void PresentationAvailabilityCallbacks::Resolve(bool value) {
  if (!resolver_->GetExecutionContext() ||
      resolver_->GetExecutionContext()->IsContextDestroyed())
    return;
  resolver_->Resolve(
      PresentationAvailability::Take(resolver_.Get(), urls_, value));
}

void PresentationAvailabilityCallbacks::RejectAvailabilityNotSupported() {
  if (!resolver_->GetExecutionContext() ||
      resolver_->GetExecutionContext()->IsContextDestroyed())
    return;
  resolver_->Reject(CreateAvailabilityNotSupportedError());
}

}  // namespace blink
