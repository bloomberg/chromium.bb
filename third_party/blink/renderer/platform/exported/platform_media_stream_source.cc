// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/modules/mediastream/platform_media_stream_source.h"

#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"

#include "base/logging.h"

namespace blink {

const char kMediaStreamSourceTab[] = "tab";
const char kMediaStreamSourceScreen[] = "screen";
const char kMediaStreamSourceDesktop[] = "desktop";
const char kMediaStreamSourceSystem[] = "system";

const char WebPlatformMediaStreamSource::kSourceId[] = "sourceId";

WebPlatformMediaStreamSource::WebPlatformMediaStreamSource() {}

WebPlatformMediaStreamSource::~WebPlatformMediaStreamSource() {
  DCHECK(stop_callback_.is_null());
  owner_ = nullptr;
}

void WebPlatformMediaStreamSource::StopSource() {
  DoStopSource();
  FinalizeStopSource();
}

void WebPlatformMediaStreamSource::FinalizeStopSource() {
  if (!stop_callback_.is_null())
    base::ResetAndReturn(&stop_callback_).Run(Owner());
  if (Owner())
    Owner().SetReadyState(blink::WebMediaStreamSource::kReadyStateEnded);
}

void WebPlatformMediaStreamSource::SetSourceMuted(bool is_muted) {
  // Although this change is valid only if the ready state isn't already Ended,
  // there's code further along (like in blink::MediaStreamTrack) which filters
  // that out already.
  if (!Owner())
    return;
  Owner().SetReadyState(is_muted
                            ? blink::WebMediaStreamSource::kReadyStateMuted
                            : blink::WebMediaStreamSource::kReadyStateLive);
}

void WebPlatformMediaStreamSource::SetDevice(
    const blink::MediaStreamDevice& device) {
  device_ = device;
}

void WebPlatformMediaStreamSource::SetStopCallback(
    const SourceStoppedCallback& stop_callback) {
  DCHECK(stop_callback_.is_null());
  stop_callback_ = stop_callback;
}

void WebPlatformMediaStreamSource::ResetSourceStoppedCallback() {
  DCHECK(!stop_callback_.is_null());
  stop_callback_.Reset();
}

void WebPlatformMediaStreamSource::ChangeSource(
    const blink::MediaStreamDevice& new_device) {
  DoChangeSource(new_device);
}

WebMediaStreamSource WebPlatformMediaStreamSource::Owner() {
  DCHECK(owner_);
  return WebMediaStreamSource(owner_.Get());
}

#if INSIDE_BLINK
void WebPlatformMediaStreamSource::SetOwner(MediaStreamSource* owner) {
  DCHECK(!owner_);
  owner_ = owner;
}
#endif

}  // namespace blink
