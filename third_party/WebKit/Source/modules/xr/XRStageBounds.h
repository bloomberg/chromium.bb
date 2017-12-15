// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XRStageBounds_h
#define XRStageBounds_h

#include "modules/xr/XRStageBoundsPoint.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class XRStageBounds final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRStageBounds() = default;

  HeapVector<Member<XRStageBoundsPoint>> geometry() const { return geometry_; }

  virtual void Trace(blink::Visitor*);

 private:
  HeapVector<Member<XRStageBoundsPoint>> geometry_;
};

}  // namespace blink

#endif  // XRStageBounds_h
