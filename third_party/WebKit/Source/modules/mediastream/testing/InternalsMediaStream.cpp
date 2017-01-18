// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediastream/testing/InternalsMediaStream.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"

namespace blink {

ScriptPromise InternalsMediaStream::addFakeDevice(
    ScriptState* scriptState,
    Internals&,
    const MediaDeviceInfo* deviceInfo,
    const MediaTrackConstraints& capabilities,
    const MediaStreamTrack* dataSource) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  resolver->reject();
  return promise;
}

}  // namespace blink
