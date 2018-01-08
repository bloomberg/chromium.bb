// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationAvailabilityCallbacks.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "modules/presentation/PresentationAvailability.h"
#include "modules/presentation/PresentationError.h"
#include "modules/presentation/PresentationRequest.h"
#include "public/platform/modules/presentation/WebPresentationError.h"

namespace blink {

PresentationAvailabilityCallbacksImpl::PresentationAvailabilityCallbacksImpl(
    PresentationAvailabilityProperty* resolver,
    const Vector<KURL>& urls)
    : resolver_(resolver), urls_(urls) {
  DCHECK(resolver_);
}

PresentationAvailabilityCallbacksImpl::
    ~PresentationAvailabilityCallbacksImpl() = default;

void PresentationAvailabilityCallbacksImpl::OnSuccess(bool value) {
  if (!resolver_->GetExecutionContext() ||
      resolver_->GetExecutionContext()->IsContextDestroyed())
    return;
  resolver_->Resolve(
      PresentationAvailability::Take(resolver_.Get(), urls_, value));
}

void PresentationAvailabilityCallbacksImpl::OnError(
    const WebPresentationError& error) {
  if (!resolver_->GetExecutionContext() ||
      resolver_->GetExecutionContext()->IsContextDestroyed())
    return;
  resolver_->Reject(PresentationError::Take(error));
}

}  // namespace blink
