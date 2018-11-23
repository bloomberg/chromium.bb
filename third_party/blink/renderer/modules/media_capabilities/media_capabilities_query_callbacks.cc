// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_capabilities/media_capabilities_query_callbacks.h"

#include "third_party/blink/renderer/modules/media_capabilities/media_capabilities_info.h"

namespace blink {

MediaCapabilitiesQueryCallbacks::MediaCapabilitiesQueryCallbacks(
    ScriptPromiseResolver* resolver)
    : resolver_(resolver) {}

MediaCapabilitiesQueryCallbacks::~MediaCapabilitiesQueryCallbacks() = default;

void MediaCapabilitiesQueryCallbacks::OnSuccess(
    std::unique_ptr<WebMediaCapabilitiesInfo> result) {
  if (!resolver_->GetExecutionContext() ||
      resolver_->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  Persistent<MediaCapabilitiesInfo> info(MediaCapabilitiesInfo::Create());
  info->setSupported(result->supported);
  info->setSmooth(result->smooth);
  info->setPowerEfficient(result->power_efficient);

  resolver_->Resolve(std::move(info));
}

void MediaCapabilitiesQueryCallbacks::OnError() {
  NOTREACHED();
}

}  // namespace blink
