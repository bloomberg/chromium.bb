// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_POSE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_POSE_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

class XRRigidTransform;

class XRPose : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRPose(const TransformationMatrix&, bool emulated_position);
  ~XRPose() override = default;

  XRRigidTransform* transform() const { return transform_; }
  bool emulatedPosition() const { return emulated_position_; }

  void Trace(Visitor*) const override;

 protected:
  Member<XRRigidTransform> transform_;

 private:
  bool emulated_position_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_POSE_H_
