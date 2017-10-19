// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SourceBufferTrackBaseSupplement_h
#define SourceBufferTrackBaseSupplement_h

#include "platform/Supplementable.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class TrackBase;
class SourceBuffer;

class SourceBufferTrackBaseSupplement
    : public GarbageCollected<SourceBufferTrackBaseSupplement>,
      public Supplement<TrackBase> {
  USING_GARBAGE_COLLECTED_MIXIN(SourceBufferTrackBaseSupplement);

 public:
  static SourceBuffer* sourceBuffer(TrackBase&);
  static void SetSourceBuffer(TrackBase&, SourceBuffer*);

  virtual void Trace(blink::Visitor*);

 private:
  static SourceBufferTrackBaseSupplement& From(TrackBase&);
  static SourceBufferTrackBaseSupplement* FromIfExists(TrackBase&);

  Member<SourceBuffer> source_buffer_;
};

}  // namespace blink

#endif
