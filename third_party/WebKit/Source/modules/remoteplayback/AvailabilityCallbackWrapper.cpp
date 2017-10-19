// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/remoteplayback/AvailabilityCallbackWrapper.h"

#include "bindings/modules/v8/v8_remote_playback_availability_callback.h"
#include "modules/remoteplayback/RemotePlayback.h"

namespace blink {

AvailabilityCallbackWrapper::AvailabilityCallbackWrapper(
    V8RemotePlaybackAvailabilityCallback* callback)
    : bindings_cb_(callback) {}

AvailabilityCallbackWrapper::AvailabilityCallbackWrapper(WTF::Closure callback)
    : internal_cb_(std::move(callback)) {}

void AvailabilityCallbackWrapper::Run(RemotePlayback* remote_playback,
                                      bool new_availability) {
  if (internal_cb_) {
    DCHECK(!bindings_cb_);
    internal_cb_();
    return;
  }

  bindings_cb_->call(remote_playback, new_availability);
}

void AvailabilityCallbackWrapper::Trace(blink::Visitor* visitor) {
  visitor->Trace(bindings_cb_);
}

DEFINE_TRACE_WRAPPERS(AvailabilityCallbackWrapper) {
  visitor->TraceWrappers(bindings_cb_);
}

}  // namespace blink
