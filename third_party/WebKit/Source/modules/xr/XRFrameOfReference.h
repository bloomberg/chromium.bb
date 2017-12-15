// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XRFrameOfReference_h
#define XRFrameOfReference_h

#include "modules/xr/XRCoordinateSystem.h"
#include "platform/transforms/TransformationMatrix.h"

namespace blink {

class XRStageBounds;

class XRFrameOfReference final : public XRCoordinateSystem {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum Type { kTypeHeadModel, kTypeEyeLevel, kTypeStage };

  XRFrameOfReference(XRSession*, Type);
  ~XRFrameOfReference() override;

  void UpdatePoseTransform(std::unique_ptr<TransformationMatrix>);
  void UpdateStageBounds(XRStageBounds*);
  void UseEmulatedHeight(double value);

  std::unique_ptr<TransformationMatrix> TransformBasePose(
      const TransformationMatrix& base_pose) override;

  XRStageBounds* bounds() const { return bounds_; }
  double emulatedHeight() const { return emulatedHeight_; }

  Type type() const { return type_; }

  void Trace(blink::Visitor*) override;

 private:
  Member<XRStageBounds> bounds_;
  double emulatedHeight_ = 0.0;
  Type type_;
  std::unique_ptr<TransformationMatrix> pose_transform_;
};

}  // namespace blink

#endif  // XRWebGLLayer_h
