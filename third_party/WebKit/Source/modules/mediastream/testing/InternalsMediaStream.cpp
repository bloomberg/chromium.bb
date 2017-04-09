// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediastream/testing/InternalsMediaStream.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"

namespace blink {

ScriptPromise InternalsMediaStream::addFakeDevice(
    ScriptState* script_state,
    Internals&,
    const MediaDeviceInfo* device_info,
    const MediaTrackConstraints& capabilities,
    const MediaStreamTrack* data_source) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  resolver->Reject();
  return promise;
}

}  // namespace blink
