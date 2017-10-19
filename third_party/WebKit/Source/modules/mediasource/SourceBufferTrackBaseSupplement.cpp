// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediasource/SourceBufferTrackBaseSupplement.h"

#include "core/html/track/TrackBase.h"
#include "modules/mediasource/SourceBuffer.h"

namespace blink {

static const char kSupplementName[] = "SourceBufferTrackBaseSupplement";

// static
SourceBufferTrackBaseSupplement* SourceBufferTrackBaseSupplement::FromIfExists(
    TrackBase& track) {
  return static_cast<SourceBufferTrackBaseSupplement*>(
      Supplement<TrackBase>::From(track, kSupplementName));
}

// static
SourceBufferTrackBaseSupplement& SourceBufferTrackBaseSupplement::From(
    TrackBase& track) {
  SourceBufferTrackBaseSupplement* supplement = FromIfExists(track);
  if (!supplement) {
    supplement = new SourceBufferTrackBaseSupplement();
    Supplement<TrackBase>::ProvideTo(track, kSupplementName, supplement);
  }
  return *supplement;
}

// static
SourceBuffer* SourceBufferTrackBaseSupplement::sourceBuffer(TrackBase& track) {
  SourceBufferTrackBaseSupplement* supplement = FromIfExists(track);
  if (supplement)
    return supplement->source_buffer_;
  return nullptr;
}

void SourceBufferTrackBaseSupplement::SetSourceBuffer(
    TrackBase& track,
    SourceBuffer* source_buffer) {
  From(track).source_buffer_ = source_buffer;
}

void SourceBufferTrackBaseSupplement::Trace(blink::Visitor* visitor) {
  visitor->Trace(source_buffer_);
  Supplement<TrackBase>::Trace(visitor);
}

}  // namespace blink
