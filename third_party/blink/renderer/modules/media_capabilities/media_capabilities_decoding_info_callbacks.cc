// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_capabilities/media_capabilities_decoding_info_callbacks.h"

#include "third_party/blink/renderer/modules/media_capabilities/media_capabilities_decoding_info.h"

namespace blink {

MediaCapabilitiesDecodingInfoCallbacks::MediaCapabilitiesDecodingInfoCallbacks(
    ScriptPromiseResolver* resolver)
    : resolver_(resolver) {}

MediaCapabilitiesDecodingInfoCallbacks::
    ~MediaCapabilitiesDecodingInfoCallbacks() = default;

void MediaCapabilitiesDecodingInfoCallbacks::OnSuccess(
    std::unique_ptr<WebMediaCapabilitiesInfo> result) {
  if (!resolver_->GetExecutionContext() ||
      resolver_->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  Persistent<MediaCapabilitiesDecodingInfo> info(
      MediaCapabilitiesDecodingInfo::Create());
  info->setSupported(result->supported);
  info->setSmooth(result->smooth);
  info->setPowerEfficient(result->power_efficient);

  resolver_->Resolve(std::move(info));
}

void MediaCapabilitiesDecodingInfoCallbacks::OnError() {
  NOTREACHED();
}

}  // namespace blink
