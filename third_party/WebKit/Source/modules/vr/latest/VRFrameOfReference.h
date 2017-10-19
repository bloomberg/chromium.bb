// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRFrameOfReference_h
#define VRFrameOfReference_h

#include "modules/vr/latest/VRCoordinateSystem.h"
#include "platform/transforms/TransformationMatrix.h"

namespace blink {

class VRStageBounds;

class VRFrameOfReference final : public VRCoordinateSystem {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum Type { kTypeHeadModel, kTypeEyeLevel, kTypeStage };

  VRFrameOfReference(VRSession*, Type);
  ~VRFrameOfReference() override;

  void UpdatePoseTransform(std::unique_ptr<TransformationMatrix>);
  void UpdateStageBounds(VRStageBounds*);
  void UseEmulatedHeight(double value);

  std::unique_ptr<TransformationMatrix> TransformBasePose(
      const TransformationMatrix& base_pose) override;

  VRStageBounds* bounds() const { return bounds_; }
  double emulatedHeight() const { return emulatedHeight_; }

  Type type() const { return type_; }

  virtual void Trace(blink::Visitor*);

 private:
  Member<VRStageBounds> bounds_;
  double emulatedHeight_ = 0.0;
  Type type_;
  std::unique_ptr<TransformationMatrix> pose_transform_;
};

}  // namespace blink

#endif  // VRWebGLLayer_h
