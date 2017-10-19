// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TrackDefaultList_h
#define TrackDefaultList_h

#include "modules/mediasource/TrackDefault.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;

class TrackDefaultList final : public GarbageCollected<TrackDefaultList>,
                               public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static TrackDefaultList* Create();  // Creates an empty TrackDefaultList.

  // Implement the IDL
  static TrackDefaultList* Create(const HeapVector<Member<TrackDefault>>&,
                                  ExceptionState&);

  unsigned length() const { return track_defaults_.size(); }
  TrackDefault* item(unsigned) const;

  void Trace(blink::Visitor*);

 private:
  TrackDefaultList();

  explicit TrackDefaultList(const HeapVector<Member<TrackDefault>>&);

  const HeapVector<Member<TrackDefault>> track_defaults_;
};

}  // namespace blink

#endif  // TrackDefaultList_h
