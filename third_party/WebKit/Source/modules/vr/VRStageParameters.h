// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRStageParameters_h
#define VRStageParameters_h

#include "core/typed_arrays/DOMTypedArray.h"
#include "device/vr/vr_service.mojom-blink.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class VRStageParameters final : public GarbageCollected<VRStageParameters>,
                                public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  VRStageParameters();

  DOMFloat32Array* sittingToStandingTransform() const {
    return standing_transform_;
  }

  float sizeX() const { return size_x_; }
  float sizeZ() const { return size_z_; }

  void Update(const device::mojom::blink::VRStageParametersPtr&);

  virtual void Trace(blink::Visitor*);

 private:
  Member<DOMFloat32Array> standing_transform_;
  float size_x_;
  float size_z_;
};

}  // namespace blink

#endif  // VRStageParameters_h
