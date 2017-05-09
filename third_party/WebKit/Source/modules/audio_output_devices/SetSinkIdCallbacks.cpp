// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/audio_output_devices/SetSinkIdCallbacks.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "modules/audio_output_devices/HTMLMediaElementAudioOutputDevice.h"

namespace blink {

namespace {

DOMException* ToException(WebSetSinkIdError error) {
  switch (error) {
    case WebSetSinkIdError::kNotFound:
      return DOMException::Create(kNotFoundError, "Requested device not found");
    case WebSetSinkIdError::kNotAuthorized:
      return DOMException::Create(kSecurityError,
                                  "No permission to use requested device");
    case WebSetSinkIdError::kAborted:
      return DOMException::Create(
          kAbortError, "The operation could not be performed and was aborted");
    case WebSetSinkIdError::kNotSupported:
      return DOMException::Create(kNotSupportedError,
                                  "Operation not supported");
    default:
      NOTREACHED();
      return DOMException::Create(kAbortError, "Invalid error code");
  }
}

}  // namespace

SetSinkIdCallbacks::SetSinkIdCallbacks(ScriptPromiseResolver* resolver,
                                       HTMLMediaElement& element,
                                       const String& sink_id)
    : resolver_(resolver), element_(element), sink_id_(sink_id) {
  DCHECK(resolver_);
}

SetSinkIdCallbacks::~SetSinkIdCallbacks() {}

void SetSinkIdCallbacks::OnSuccess() {
  if (!resolver_->GetExecutionContext() ||
      resolver_->GetExecutionContext()->IsContextDestroyed())
    return;

  HTMLMediaElementAudioOutputDevice& aod_element =
      HTMLMediaElementAudioOutputDevice::From(*element_);
  aod_element.setSinkId(sink_id_);
  resolver_->Resolve();
}

void SetSinkIdCallbacks::OnError(WebSetSinkIdError error) {
  if (!resolver_->GetExecutionContext() ||
      resolver_->GetExecutionContext()->IsContextDestroyed())
    return;

  resolver_->Reject(ToException(error));
}

}  // namespace blink
