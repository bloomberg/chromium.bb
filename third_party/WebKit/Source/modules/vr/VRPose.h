// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRPose_h
#define VRPose_h

#include "core/typed_arrays/DOMTypedArray.h"
#include "device/vr/vr_service.mojom-blink.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class VRPose final : public GarbageCollected<VRPose>, public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static VRPose* Create() { return new VRPose(); }

  DOMFloat32Array* orientation() const { return orientation_; }
  DOMFloat32Array* position() const { return position_; }
  DOMFloat32Array* angularVelocity() const { return angular_velocity_; }
  DOMFloat32Array* linearVelocity() const { return linear_velocity_; }
  DOMFloat32Array* angularAcceleration() const { return angular_acceleration_; }
  DOMFloat32Array* linearAcceleration() const { return linear_acceleration_; }

  void SetPose(const device::mojom::blink::VRPosePtr&);

  DECLARE_VIRTUAL_TRACE();

 private:
  VRPose();

  Member<DOMFloat32Array> orientation_;
  Member<DOMFloat32Array> position_;
  Member<DOMFloat32Array> angular_velocity_;
  Member<DOMFloat32Array> linear_velocity_;
  Member<DOMFloat32Array> angular_acceleration_;
  Member<DOMFloat32Array> linear_acceleration_;
};

}  // namespace blink

#endif  // VRPose_h
