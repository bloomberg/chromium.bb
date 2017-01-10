// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediarecorder/BlobEvent.h"

#include "modules/mediarecorder/BlobEventInit.h"
#include "wtf/dtoa/double.h"

namespace blink {

// static
BlobEvent* BlobEvent::create(const AtomicString& type,
                             const BlobEventInit& initializer) {
  return new BlobEvent(type, initializer);
}

// static
BlobEvent* BlobEvent::create(const AtomicString& type,
                             Blob* blob,
                             double timecode) {
  return new BlobEvent(type, blob, timecode);
}

const AtomicString& BlobEvent::interfaceName() const {
  return EventNames::BlobEvent;
}

DEFINE_TRACE(BlobEvent) {
  visitor->trace(m_blob);
  Event::trace(visitor);
}

BlobEvent::BlobEvent(const AtomicString& type, const BlobEventInit& initializer)
    : Event(type, initializer),
      m_blob(initializer.data()),
      m_timecode(initializer.hasTimecode()
                     ? initializer.timecode()
                     : WTF::double_conversion::Double::NaN()) {}

BlobEvent::BlobEvent(const AtomicString& type, Blob* blob, double timecode)
    : Event(type, false /* canBubble */, false /* cancelable */),
      m_blob(blob),
      m_timecode(timecode) {}

}  // namespace blink
