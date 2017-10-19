// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRStageBounds_h
#define VRStageBounds_h

#include "modules/vr/latest/VRStageBoundsPoint.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class VRStageBounds final : public GarbageCollected<VRStageBounds>,
                            public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  VRStageBounds() = default;

  HeapVector<Member<VRStageBoundsPoint>> geometry() const { return geometry_; }

  virtual void Trace(blink::Visitor*);

 private:
  HeapVector<Member<VRStageBoundsPoint>> geometry_;
};

}  // namespace blink

#endif  // VRStageBounds_h
