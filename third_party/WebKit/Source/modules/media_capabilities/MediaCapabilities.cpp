// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_capabilities/MediaCapabilities.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "modules/media_capabilities/MediaConfiguration.h"
#include "modules/media_capabilities/MediaDecodingAbility.h"

namespace blink {

MediaCapabilities::MediaCapabilities() = default;

ScriptPromise MediaCapabilities::query(
    ScriptState* scriptState,
    const MediaConfiguration& configuration) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  resolver->resolve(new MediaDecodingAbility());
  return promise;
}

DEFINE_TRACE(MediaCapabilities) {}

}  // namespace blink
