// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediarecorder/BlobEvent.h"

#include "modules/mediarecorder/BlobEventInit.h"
#include "platform/wtf/dtoa/double.h"

namespace blink {

// static
BlobEvent* BlobEvent::Create(const AtomicString& type,
                             const BlobEventInit& initializer) {
  return new BlobEvent(type, initializer);
}

// static
BlobEvent* BlobEvent::Create(const AtomicString& type,
                             Blob* blob,
                             double timecode) {
  return new BlobEvent(type, blob, timecode);
}

const AtomicString& BlobEvent::InterfaceName() const {
  return EventNames::BlobEvent;
}

void BlobEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(blob_);
  Event::Trace(visitor);
}

BlobEvent::BlobEvent(const AtomicString& type, const BlobEventInit& initializer)
    : Event(type, initializer),
      blob_(initializer.data()),
      timecode_(initializer.hasTimecode()
                    ? initializer.timecode()
                    : WTF::double_conversion::Double::NaN()) {}

BlobEvent::BlobEvent(const AtomicString& type, Blob* blob, double timecode)
    : Event(type, false /* canBubble */, false /* cancelable */),
      blob_(blob),
      timecode_(timecode) {}

}  // namespace blink
