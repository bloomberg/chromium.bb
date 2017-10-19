// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRStageBoundsPoint_h
#define VRStageBoundsPoint_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class VRStageBoundsPoint final : public GarbageCollected<VRStageBoundsPoint>,
                                 public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  VRStageBoundsPoint(double x, double z) : x_(x), z_(z) {}

  double x() const { return x_; }
  double z() const { return z_; }

  void Trace(blink::Visitor* visitor) {}

 private:
  double x_;
  double z_;
};

}  // namespace blink

#endif  // VRStageBoundsPoint_h
